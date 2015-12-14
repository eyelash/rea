/*

Copyright (c) 2015, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "writer.hpp"

Void Type::VOID;
Bool Type::BOOL;
Int Type::INT;

void Number::insert (Writer& writer) {
	writer.write (n);
}

void BooleanLiteral::insert (Writer& writer) {
	writer.write (value ? 1 : 0);
}

void Variable::evaluate (Writer& writer) {
	value = writer.get_next_value ();
	writer.write (INDENT "%%% = load %* %%v%\n", value, type, n);
}
void Variable::insert (Writer& writer) {
	writer.write ("%%%", value);
}
void Variable::insert_address (Writer& writer) {
	writer.write ("%%v%", n);
}

void Assignment::evaluate (Writer& writer) {
	right->evaluate (writer);
	writer.write (INDENT "store % %, %* %\n", right->get_type(), right, left->get_type(), left->get_address());
}
void Assignment::insert (Writer& writer) {
	right->insert (writer);
}

void Call::evaluate (Writer& writer) {
	for (Expression* argument: arguments) {
		argument->evaluate (writer);
	}
	if (function->get_return_type() == &Type::VOID) {
		writer.write (INDENT "call void @%(", function->get_name());
	}
	else {
		value = writer.get_next_value ();
		writer.write (INDENT "%%% = call % @%(", value, function->get_return_type(), function->get_name());
	}
	auto i = arguments.begin ();
	if (i != arguments.end()) {
		writer.write ("% %", (*i)->get_type(), *i);
		++i;
	}
	while (i != arguments.end()) {
		writer.write (", % %", (*i)->get_type(), *i);
		++i;
	}
	writer.write (")\n");
}
void Call::insert (Writer& writer) {
	writer.write ("%%%", value);
}
bool Call::validate () {
	if (arguments.size() != function->get_arguments().size()) return false;
	for (int i = 0; i < arguments.size(); i++) {
		if (arguments[i]->get_type() != function->get_arguments()[i]->get_type()) return false;
	}
	return true;
}

void BinaryExpression::evaluate (Writer& writer) {
	left->evaluate (writer);
	right->evaluate (writer);
	value = writer.get_next_value ();
	writer.write (INDENT "%%% = % i32 %, %\n", value, instruction, left, right);
}
void BinaryExpression::insert (Writer& writer) {
	writer.write ("%%%", value);
}

void If::write (Writer& writer) {
	int if_label = writer.get_next_label ();
	int endif_label = writer.get_next_label ();
	
	condition->evaluate (writer);
	writer.write (INDENT "br i1 %, label %%l%, label %%l%\n", condition, if_label, endif_label);
	
	// if
	writer.write ("l%:\n", if_label);
	writer.write (if_block);
	if (!if_block->returns) writer.write (INDENT "br label %%l%\n", endif_label);
	
	// endif
	writer.write ("l%:\n", endif_label);
}

void While::write (Writer& writer) {
	int checkwhile_label = writer.get_next_label ();
	int while_label = writer.get_next_label ();
	int endwhile_label = writer.get_next_label ();
	
	writer.write (INDENT "br label %%l%\n", checkwhile_label);
	
	// checkwhile
	writer.write ("l%:\n", checkwhile_label);
	condition->evaluate (writer);
	writer.write (INDENT "br i1 %, label %%l%, label %%l%\n", condition, while_label, endwhile_label);
	
	// while
	writer.write ("l%:\n", while_label);
	writer.write (block);
	if (!block->returns) writer.write (INDENT "br label %%l%\n", checkwhile_label);
	
	// endwhile
	writer.write ("l%:\n", endwhile_label);
}

void Return::write (Writer& writer) {
	if (expression) {
		expression->evaluate (writer);
		writer.write (INDENT "ret % %\n", expression->get_type(), expression);
	}
	else writer.write (INDENT "ret void\n");
}

void FunctionDeclaration::write (Writer& writer) {
	writer.write ("declare % @%(", return_type, name);
	auto i = arguments.begin ();
	if (i != arguments.end()) {
		writer.write ((*i)->get_type());
		++i;
		while (i != arguments.end()) {
			writer.write (", %", (*i)->get_type());
			++i;
		}
	}
	writer.write (")\n\n");
}

void Function::write (Writer& writer) {
	writer.write ("define % @%(", return_type, name);
	auto i = arguments.begin ();
	if (i != arguments.end()) {
		writer.write ("% %%a%", (*i)->get_type(), (*i)->get_n());
		++i;
	}
	while (i != arguments.end()) {
		writer.write (", % %%a%", (*i)->get_type(), (*i)->get_n());
		++i;
	}
	writer.write (") nounwind {\n");
	writer.reset ();
	for (Variable* variable: variables) {
		writer.write (INDENT "% = alloca %\n", variable->get_address(), variable->get_type());
	}
	for (Variable* argument: arguments) {
		writer.write (INDENT "store % %%a%, %* %\n", argument->get_type(), argument->get_n(), argument->get_type(), argument->get_address());
	}
	writer.write (block);
	if (!block->returns) writer.write (INDENT "ret void\n");
	writer.write ("}\n\n");
}

void Block::write (Writer& writer) {
	for (Node* node: nodes) {
		writer.write (node);
	}
}

void Class::write (Writer& writer) {
	writer.write ("%%% = type {\n", name);
	auto i = attributes.begin ();
	if (i != attributes.end()) {
		writer.write (INDENT "%", i->second->get_type());
		++i;
		while (i != attributes.end()) {
			writer.write (",\n" INDENT "%", i->second->get_type());
			++i;
		}
	}
	writer.write ("\n}\n\n");
}
void Class::insert (Writer& writer) const {
	writer.write ("%%%", name);
}

void Program::write (Writer& writer) {
	for (FunctionDeclaration* function_declaration: function_declarations) function_declaration->write (writer);
	
	for (Class* _class: classes) _class->write (writer);
	
	for (Function* function: functions) function->write (writer);
}
