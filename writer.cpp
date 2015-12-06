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

void Number::insert (Writer& w) {
	w << n;
}

void Variable::evaluate (Writer& writer) {
	value = writer.get_next_value ();
	writer << INDENT << "%" << value << " = load " << type << "* %v" << n << "\n";
}
void Variable::insert (Writer& writer) {
	writer << "%" << value;
}
void Variable::insert_address (Writer& writer) {
	writer << "%v" << n;
}

void Call::evaluate (Writer& writer) {
	for (Expression* argument: arguments) {
		argument->evaluate (writer);
	}
	value = writer.get_next_value ();
	writer << INDENT << "%" << value << " = call i32 @" << function->get_name() << "(";
	auto i = arguments.begin ();
	if (i != arguments.end()) {
		writer << (*i)->get_type() << " " << *i;
		++i;
	}
	while (i != arguments.end()) {
		writer << ", " << (*i)->get_type() << " " << *i;
		++i;
	}
	writer << ")\n";
}
void Call::insert (Writer& writer) {
	writer << "%" << value;
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
	writer << INDENT << "%" << value << " = " << instruction << " i32 " << left << ", " << right;
	writer << "\n";
}
void BinaryExpression::insert (Writer& writer) {
	writer << "%" << value;
}

void Assignment::write (Writer& writer) {
	expression->evaluate (writer);
	writer << INDENT << "store " << expression->get_type() << " " << expression << ", " << variable->get_type() << "* ";
	variable->insert_address (writer);
	writer << "\n";
}

void If::write (Writer& writer) {
	int if_label = writer.get_next_label ();
	int endif_label = writer.get_next_label ();
	
	condition->evaluate (writer);
	writer << INDENT << "br i1 " << condition << ", label %l" << if_label << ", label %l" << endif_label << "\n";
	
	// if
	writer << "l" << if_label << ":\n";
	for (Node* node: nodes) {
		writer << node;
	}
	writer << INDENT << "br label %l" << endif_label << "\n";
	
	// endif
	writer << "l" << endif_label << ":\n";
}

void While::write (Writer& writer) {
	int checkwhile_label = writer.get_next_label ();
	int while_label = writer.get_next_label ();
	int endwhile_label = writer.get_next_label ();
	
	writer << INDENT << "br label %l" << checkwhile_label << "\n";
	
	// checkwhile
	writer << "l" << checkwhile_label << ":\n";
	condition->evaluate (writer);
	writer << INDENT << "br i1 " << condition << ", label %l" << while_label << ", label %l" << endwhile_label << "\n";
	
	// while
	writer << "l" << while_label << ":\n";
	for (Node* node: nodes) {
		writer << node;
	}
	writer << INDENT << "br label %l" << checkwhile_label << "\n";
	
	// endwhile
	writer << "l" << endwhile_label << ":\n";
}

void Function::write (Writer& writer) {
	writer << "define i32 @" << name << "(";
	auto i = arguments.begin ();
	if (i != arguments.end()) {
		writer << (*i)->get_type() << " %a" << (*i)->get_n();
		++i;
	}
	while (i != arguments.end()) {
		writer << ", " << (*i)->get_type() << " %a" << (*i)->get_n();
		++i;
	}
	writer << ") nounwind {\n";
	writer.reset ();
	for (Variable* variable: variables) {
		writer << INDENT;
		variable->insert_address (writer);
		writer << " = alloca " << variable->get_type() << "\n";
	}
	for (Variable* argument: arguments) {
		writer << INDENT << "store " << argument->get_type() << " %a" << argument->get_n() << ", " << argument->get_type() << "* ";
		argument->insert_address (writer);
		writer << "\n";
	}
	for (Node* node: nodes) {
		writer << node;
	}
	writer << INDENT << "ret i32 0\n";
	writer << "}\n\n";
}

void Program::write (Writer& writer) {
	writer << "declare i32 @print(i32)" << "\n\n";
	for (Node* node: nodes) {
		writer << node;
	}
}
