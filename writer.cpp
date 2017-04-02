/*

Copyright (c) 2015-2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "writer.hpp"

const ast::Void ast::Type::VOID {};
const ast::Bool ast::Type::BOOL {};
const ast::Int ast::Type::INT {};

void ast::FunctionPrototype::insert_mangled_name (File& file) {
	file.print (get_name());
	for (int i = 0; const Type* argument = get_argument(i); ++i)
		file.print (".%", argument->get_name());
}

writer::Value* ast::Number::insert (Writer& writer) {
	return writer.insert_literal (n);
}

writer::Value* ast::BooleanLiteral::insert (Writer& writer) {
	return writer.insert_literal (value ? 1 : 0);
}

writer::Value* ast::Variable::insert (Writer& writer) {
	return writer.insert_load (value, type);
}
writer::Value* ast::Variable::insert_address (Writer& writer) {
	return value;
}

writer::Value* ast::Instantiation::insert (Writer& writer) {
	writer::Value* result = writer.insert_alloca_value (_class);
	for (int i = 0; i < attribute_values.size(); ++i) {
		writer::Value* destination = writer.insert_gep (result, _class, i);
		writer::Value* source = attribute_values[i]->insert (writer);
		writer.insert_store (destination, source, attribute_values[i]->get_type());
	}
	return result;
}

writer::Value* ast::AttributeAccess::insert (Writer& writer) {
	writer::Value* address = insert_address (writer);
	return writer.insert_load (address, get_type());
}
writer::Value* ast::AttributeAccess::insert_address (Writer& writer) {
	writer::Value* value = expression->insert (writer);
	int index = expression->get_type()->get_class()->get_attribute(name)->get_n ();
	return writer.insert_gep (value, expression->get_type(), index);
}

writer::Value* ast::Assignment::insert (Writer& writer) {
	writer::Value* destination = left->insert_address (writer);
	writer::Value* source = right->insert (writer);
	writer.insert_store (destination, source, get_type());
	return nullptr;
}

writer::Value* ast::Call::insert (Writer& writer) {
	std::vector<writer::Value*> argument_values;
	for (Expression* argument: arguments) {
		argument_values.push_back (argument->insert(writer));
	}
	return writer.insert_call (this, argument_values);
}

writer::Value* ast::BinaryExpression::insert (Writer& writer) {
	writer::Value* left_value = left->insert (writer);
	writer::Value* right_value = right->insert (writer);
	return writer.insert_binary_operation (instruction, left_value, right_value);
}

writer::Value* ast::And::insert (Writer& writer) {
	writer::Block* block0 = writer.get_current_block ();
	writer::Block* block1 = writer.create_block ();
	writer::Block* block2 = writer.create_block ();
	
	writer::Value* value0 = left->insert (writer);
	writer.insert_branch (block1, block2, value0);
	
	writer.insert_block (block1);
	writer::Value* value1 = right->insert (writer);
	writer.insert_branch (block2);
	
	writer.insert_block (block2);
	return writer.insert_phi (&ast::Type::BOOL, value0, block0, value1, block1);
}

writer::Value* ast::Or::insert (Writer& writer) {
	writer::Block* block0 = writer.get_current_block ();
	writer::Block* block1 = writer.create_block ();
	writer::Block* block2 = writer.create_block ();
	
	writer::Value* value0 = left->insert (writer);
	writer.insert_branch (block2, block1, value0);
	
	writer.insert_block (block1);
	writer::Value* value1 = right->insert (writer);
	writer.insert_branch (block2);
	
	writer.insert_block (block2);
	return writer.insert_phi (&ast::Type::BOOL, value0, block0, value1, block1);
}

void ast::If::write (Writer& writer) {
	writer::Block* _if = writer.create_block ();
	writer::Block* _endif = writer.create_block ();
	
	writer::Value* _c = condition->insert (writer);
	writer.insert_branch (_if, _endif, _c);
	
	writer.insert_block (_if);
	if_block->write (writer);
	if (!if_block->returns) writer.insert_branch (_endif);
	
	writer.insert_block (_endif);
}

void ast::While::write (Writer& writer) {
	writer::Block* checkwhile = writer.create_block ();
	writer::Block* _while = writer.create_block ();
	writer::Block* endwhile = writer.create_block ();
	
	writer.insert_branch (checkwhile);
	
	writer.insert_block (checkwhile);
	writer::Value* _c = condition->insert (writer);
	writer.insert_branch (_while, endwhile, _c);
	
	writer.insert_block (_while);
	block->write (writer);
	if (!block->returns) writer.insert_branch (checkwhile);
	
	writer.insert_block (endwhile);
}

void ast::Return::write (Writer& writer) {
	if (expression)
		writer.insert_return (expression->insert(writer), expression->get_type());
	else
		writer.insert_return ();
}

void ast::Function::write (Writer& writer) {
	std::vector<writer::Value*> argument_values = writer.insert_function (this);
	for (Variable* variable: variables) {
		variable->value = writer.insert_alloca (variable->get_type());
	}
	for (int i = 0; i < arguments.size(); ++i) {
		writer.insert_store (arguments[i]->value, argument_values[i], arguments[i]->get_type());
	}
	block->write (writer);
	if (!block->returns) writer.insert_return ();
}

void ast::Block::write (Writer& writer) {
	for (Node* node: nodes) {
		node->write (writer);
	}
}

void ast::Program::write (Writer& writer) {
	for (FunctionDeclaration* function_declaration: function_declarations) writer.insert_function_declaration (function_declaration);
	
	for (Class* _class: classes) writer.insert_class (_class);
	
	for (Function* function: functions) function->write (writer);
}

// writer

template <class F> writer::Instruction* make_instruction (const F& f) {
	class LambdaInstruction: public writer::Instruction {
		F f;
	public:
		LambdaInstruction (const F& f): f(f) {}
		void print (File& file) const override {
			f (file);
		}
	};
	return new LambdaInstruction (f);
}

writer::Type* writer::get_type (const ast::Type* type) {
	class PrimitiveType: public writer::Type {
		const char* name;
	public:
		PrimitiveType (const char* name): name(name) {}
		void print (File& file) const override {
			file.print (name);
		}
	};
	class ClassType: public writer::Type {
		Substring name;
	public:
		ClassType (const Substring& name): name(name) {}
		void print (File& file) const override {
			file.print ("%%%*", name);
		}
	};
	if (type->type == nullptr) {
		if (type == &ast::Type::VOID) type->type = new PrimitiveType ("void");
		else if (type == &ast::Type::BOOL) type->type = new PrimitiveType ("i1");
		else if (type == &ast::Type::INT) type->type = new PrimitiveType ("i32");
		else if (const ast::Class* _class = type->get_class()) type->type = new ClassType (_class->get_name());
	}
	return type->type;
}

void writer::Block::write (File& file) {
	file.print ("; %%%:\n", n);
	for (writer::Instruction* instruction: instructions) {
		file.print (INDENT "%\n", instruction);
	}
}

void writer::Function::write (File& file) {
	file.print ("define % @%(", writer::get_type(function->get_return_type()), function->get_mangled_name());
	if (const ast::Type* argument = function->get_argument(0)) {
		file.print (writer::get_type(argument));
		for (int i = 1; const ast::Type* argument = function->get_argument(i); ++i) {
			file.print (", %", writer::get_type(argument));
		}
	}
	file.print (") nounwind {\n");
	for (writer::Block* block: blocks) {
		block->write (file);
	}
	file.print ("}\n\n");
}

// Writer

writer::Value* Writer::insert_literal (int n) {
	class LiteralValue: public writer::Value {
		int n;
	public:
		LiteralValue (int n): n(n) {}
		void print (File& file) const override {
			file.print (n);
		}
	};
	return new LiteralValue (n);
}

writer::Value* Writer::insert_load (writer::Value* value, const ast::Type* _type) {
	writer::Value* destination = next_value ();
	const writer::Type* type = writer::get_type (_type);
	insert_instruction (make_instruction([=] (File& file) {
		file.print ("% = load %* %", destination, type, value);
	}));
	return destination;
}

void Writer::insert_store (writer::Value* destination, writer::Value* source, const ast::Type* _type) {
	const writer::Type* type = writer::get_type (_type);
	insert_instruction (make_instruction([=] (File& file) {
		file.print ("store % %, %* %", type, source, type, destination);
	}));
}

writer::Value* Writer::insert_alloca (const writer::Type* type) {
	writer::Value* value = next_value ();
	insert_instruction (make_instruction([=] (File& file) {
		file.print ("% = alloca %", value, type);
	}));
	return value;
}

writer::Value* Writer::insert_alloca (const ast::Type* type) {
	return insert_alloca (writer::get_type(type));
}

writer::Value* Writer::insert_alloca_value (const ast::Class* _class) {
	class ValueType: public writer::Type {
		Substring name;
	public:
		ValueType (const Substring& name): name(name) {}
		void print (File& file) const override {
			file.print ("%%%", name);
		}
	};
	return insert_alloca (new ValueType(_class->get_name()));
}

writer::Value* Writer::insert_gep (writer::Value* value, const ast::Type* type, int index) {
	class GEPInstruction: public writer::Instruction {
		writer::Value* destination;
		writer::Value* source;
		const writer::Type* type;
		int index;
	public:
		GEPInstruction (writer::Value* destination, writer::Value* source, const writer::Type* type, int index): destination(destination), source(source), type(type), index(index) {}
		void print (File& file) const override {
			file.print ("% = getelementptr % %, i32 0, i32 %", destination, type, source, index);
		}
	};
	writer::Value* result = next_value ();
	insert_instruction (new GEPInstruction(result, value, writer::get_type(type), index));
	return result;
}

writer::Value* Writer::insert_call (ast::Call* call, const std::vector<writer::Value*>& arguments) {
	class CallInstruction: public writer::Instruction {
		writer::Value* value;
		ast::Call* call;
		std::vector<writer::Value*> arguments;
	public:
		CallInstruction (writer::Value* value, ast::Call* call, const std::vector<writer::Value*>& arguments): value(value), call(call), arguments(arguments) {}
		void print (File& file) const override {
			if (value)
				file.print ("% = call % @%(", value, writer::get_type(call->get_type()), call->get_mangled_name());
			else
				file.print ("call % @%(", writer::get_type(call->get_type()), call->get_mangled_name());
			if (const ast::Type* type = call->get_argument(0)) {
				file.print ("% %", writer::get_type(type), arguments[0]);
				for (int i = 1; const ast::Type* type = call->get_argument(i); ++i) {
					file.print (", % %", writer::get_type(type), arguments[i]);
				}
			}
			file.print (")");
		}
	};
	writer::Value* value = nullptr;
	if (call->get_type() != &ast::Type::VOID)
		value = next_value ();
	insert_instruction (new CallInstruction(value, call, arguments));
	return value;
}

writer::Value* Writer::insert_binary_operation (const char* operation, writer::Value* left, writer::Value* right) {
	class BinaryInstruction: public writer::Instruction {
		writer::Value* value;
		const char* operation;
		writer::Value* left;
		writer::Value* right;
	public:
		BinaryInstruction (writer::Value* value, const char* operation, writer::Value* left, writer::Value* right): value(value), operation(operation), left(left), right(right) {}
		void print (File& file) const override {
			file.print ("% = % i32 %, %", value, operation, left, right);
		}
	};
	writer::Value* value = next_value ();
	insert_instruction (new BinaryInstruction(value, operation, left, right));
	return value;
}

void Writer::insert_return (writer::Value* value, const ast::Type* type) {
	class ReturnInstruction: public writer::Instruction {
		writer::Value* value;
		const writer::Type* type;
	public:
		ReturnInstruction (writer::Value* value, const writer::Type* type): value(value), type(type) {}
		void print (File& file) const override {
			if (value) file.print ("ret % %", type, value);
			else file.print ("ret void");
		}
	};
	insert_instruction (new ReturnInstruction(value, writer::get_type(type)));
}
void Writer::insert_return () {
	insert_instruction (make_instruction([] (File& file) {
		file.print ("ret void");
	}));
}

void Writer::insert_branch (writer::Block* true_destination, writer::Block* false_destination, writer::Value* condition) {
	insert_instruction (make_instruction([=] (File& file) {
		file.print ("br i1 %, label %, label %", condition, true_destination, false_destination);
	}));
}
void Writer::insert_branch (writer::Block* destination) {
	insert_instruction (make_instruction([=] (File& file) {
		file.print ("br label %", destination);
	}));
}

writer::Value* Writer::insert_phi (const ast::Type* _type, writer::Value* value1, writer::Block* block1, writer::Value* value2, writer::Block* block2) {
	writer::Value* value = next_value ();
	const writer::Type* type = writer::get_type(_type);
	insert_instruction (make_instruction([=] (File& file) {
		file.print ("% = phi % [%, %], [%, %]", value, type, value1, block1, value2, block2);
	}));
	return value;
}

void Writer::insert_block (writer::Block* block) {
	block->n = n++;
	functions.back()->insert_block (block);
}

void Writer::insert_function_declaration (ast::FunctionDeclaration* function_declaration) {
	function_declarations.push_back (function_declaration);
}

void Writer::insert_class (ast::Class* _class) {
	classes.push_back (_class);
}
std::vector<writer::Value*> Writer::insert_function (ast::Function* function) {
	functions.push_back (new writer::Function(function));
	n = 0;
	std::vector<writer::Value*> result;
	for (int i = 0; function->get_argument(i); ++i) {
		result.push_back (next_value());
	}
	insert_block (new writer::Block());
	return result;
}


void Writer::write () {
	File file {stdout};
	
	for (ast::FunctionDeclaration* function_declaration: function_declarations) {
		file.print ("declare % @%(", writer::get_type(function_declaration->get_return_type()), function_declaration->get_mangled_name());
		if (const ast::Type* argument = function_declaration->get_argument(0)) {
			file.print (writer::get_type(argument));
			for (int i = 1; const ast::Type* argument = function_declaration->get_argument(i); ++i) {
				file.print (", %", writer::get_type(argument));
			}
		}
		file.print (")\n\n");
	}
	
	for (ast::Class* _class: classes) {
		file.print ("%%% = type {\n", _class->get_name());
		auto i = _class->get_attributes().begin ();
		if (i != _class->get_attributes().end()) {
			file.print (INDENT "%", writer::get_type((*i)->get_type()));
			++i;
			while (i != _class->get_attributes().end()) {
				file.print (",\n" INDENT "%", writer::get_type((*i)->get_type()));
				++i;
			}
		}
		file.print ("\n}\n\n");
	}
	
	for (writer::Function* function: functions) {
		function->write (file);
	}
}
