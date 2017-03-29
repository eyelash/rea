/*

Copyright (c) 2015-2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "ast.hpp"

#define INDENT "  "

namespace writer {

class Type: public Printable {
	
};

Type* get_type (const ast::Type* type);

class Value: public Printable {
	
};
class RegisterValue: public Value {
	int n;
public:
	RegisterValue (int n): n(n) {}
	void print (File& file) const override {
		file.print ("%%%", n);
	}
};

class Instruction: public Printable {
	
};

class Block: public Printable {
	std::vector<Instruction*> instructions;
public:
	int n;
	void insert_instruction (Instruction* instruction) {
		instructions.push_back (instruction);
	}
	void write (File& file);
	void print (File& file) const override {
		file.print ("%%%", n);
	}
};

class Function {
	std::vector<Block*> blocks;
public:
	ast::Function* function;
	Function (ast::Function* function): function(function) {}
	void insert_block (Block* block) {
		blocks.push_back (block);
	}
	void insert_instruction (Instruction* instruction) {
		blocks.back()->insert_instruction (instruction);
	}
	void write (File& file);
};

}

class Writer {
	std::vector<ast::FunctionDeclaration*> function_declarations;
	std::vector<ast::Class*> classes;
	std::vector<writer::Function*> functions;
	int n;
	void insert_instruction (writer::Instruction* instruction) {
		functions.back()->insert_instruction (instruction);
	}
	writer::Value* next_value (const ast::Type* type = &ast::Type::VOID) {
		return new writer::RegisterValue (n++);
	}
public:
	writer::Value* insert_literal (int n);
	writer::Value* insert_load (writer::Value* value, const ast::Type* type);
	void insert_store (writer::Value* destination, writer::Value* source, const ast::Type* type);
	writer::Value* insert_alloca (const writer::Type* type);
	writer::Value* insert_alloca (const ast::Type* type);
	writer::Value* insert_alloca_value (const ast::Class* _class);
	writer::Value* insert_gep (writer::Value* value, const ast::Type* type, int index);
	writer::Value* insert_call (ast::Call* call, const std::vector<writer::Value*>& arguments);
	writer::Value* insert_binary_operation (const char* operation, writer::Value* left, writer::Value* right);
	void insert_return (writer::Value* value, const ast::Type* type);
	void insert_return ();
	void insert_branch (writer::Block* destination);
	void insert_branch (writer::Block* true_destination, writer::Block* false_destination, writer::Value* condition);
	
	writer::Block* create_block () {
		return new writer::Block ();
	}
	void insert_block (writer::Block* block);
	void insert_function_declaration (ast::FunctionDeclaration* function_declaration);
	void insert_class (ast::Class* _class);
	std::vector<writer::Value*> insert_function (ast::Function* function);
	
	void write ();
};
