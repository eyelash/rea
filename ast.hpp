/*

Copyright (c) 2015-2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include "foundation.hpp"
#include <vector>

class Writer;
namespace writer {
	class Type;
	class Value;
}

namespace ast {

class Void;
class Bool;
class Int;
class Class;

class Type {
public:
	mutable writer::Type* type;
	Type (): type(nullptr) {}
	virtual Substring get_name () const = 0;
	virtual const Class* get_class () const { return nullptr; }
	static const Void VOID;
	static const Bool BOOL;
	static const Int INT;
};

class Void: public Type {
public:
	Substring get_name () const override { return "Void"; }
};

class Bool: public Type {
public:
	Substring get_name () const override { return "Bool"; }
};

class Int: public Type {
public:
	Substring get_name () const override { return "Int"; }
};

class Expression {
public:
	virtual writer::Value* insert (Writer&) = 0;
	virtual bool has_address () { return false; }
	virtual writer::Value* insert_address (Writer&) { return nullptr; }
	virtual const Type* get_type () = 0;
	virtual bool validate () { return true; }
};

class FunctionPrototype {
public:
	virtual const Substring& get_name () const = 0;
	virtual const Type* get_argument (int index) const = 0;
	bool operator == (const FunctionPrototype& other) const {
		if (!(get_name() == other.get_name())) return false;
		for (int i = 0; get_argument(i) || other.get_argument(i); ++i) {
			if (get_argument(i) != other.get_argument(i)) return false;
		}
		return true;
	}
	void insert_mangled_name (File& file);
	class MangledName: public Printable {
		FunctionPrototype* prototype;
	public:
		MangledName (FunctionPrototype* prototype): prototype(prototype) {}
		void print (File& file) const override {
			prototype->insert_mangled_name (file);
		}
	};
	MangledName get_mangled_name () {
		return MangledName (this);
	}
};

class Number: public Expression {
	int n;
public:
	Number (int n): n(n) {}
	writer::Value* insert (Writer& w) override;
	const Type* get_type () override {
		return &Type::INT;
	}
};

class BooleanLiteral: public Expression {
	bool value;
public:
	BooleanLiteral (bool value): value(value) {}
	writer::Value* insert (Writer& writer) override;
	const Type* get_type () override {
		return &Type::BOOL;
	}
};

class Variable: public Expression {
	Substring name;
	const Type* type;
	int n;
public:
	writer::Value* value;
	Variable (const Substring& name, const Type* type): name(name), type(type) {}
	const Substring& get_name () const {
		return name;
	}
	void set_n (int n) { this->n = n; }
	int get_n () const { return n; }
	writer::Value* insert (Writer& writer) override;
	bool has_address () override { return true; }
	writer::Value* insert_address (Writer& writer);
	const Type* get_type () override {
		return type;
	}
};

class Assignment: public Expression {
	Expression* left;
	Expression* right;
public:
	Assignment (Expression* left, Expression* right): left(left), right(right) {}
	writer::Value* insert (Writer& writer) override;
	const Type* get_type () override {
		return right->get_type ();
	}
	bool validate () override {
		return left->has_address() && left->get_type() == right->get_type();
	}
	static Expression* create (Expression* left, Expression* right) {
		return new Assignment (left, right);
	}
};

class BinaryExpression: public Expression {
	const char* instruction;
	Expression* left;
	Expression* right;
public:
	BinaryExpression (const char* instruction, Expression* left, Expression* right): instruction(instruction), left(left), right(right) {}
	writer::Value* insert (Writer& writer) override;
	const Type* get_type () override {
		return left->get_type ();
	}
	bool validate () override {
		return left->get_type() == &Type::INT && right->get_type() == &Type::INT;
	}
	static Expression* add (Expression* left, Expression* right) {
		return new BinaryExpression ("add", left, right);
	}
	static Expression* sub (Expression* left, Expression* right) {
		return new BinaryExpression ("sub", left, right);
	}
	static Expression* mul (Expression* left, Expression* right) {
		return new BinaryExpression ("mul", left, right);
	}
	static Expression* div (Expression* left, Expression* right) {
		return new BinaryExpression ("sdiv", left, right);
	}
	static Expression* mod (Expression* left, Expression* right) {
		return new BinaryExpression ("srem", left, right);
	}
};

class ComparisonExpression: public BinaryExpression {
public:
	ComparisonExpression (const char* instruction, Expression* left, Expression* right): BinaryExpression(instruction, left, right) {}
	const Type* get_type () override {
		return &Type::BOOL;
	}
	static Expression* eq (Expression* left, Expression* right) {
		return new ComparisonExpression ("icmp eq", left, right);
	}
	static Expression* ne (Expression* left, Expression* right) {
		return new ComparisonExpression ("icmp ne", left, right);
	}
	static Expression* lt (Expression* left, Expression* right) {
		return new ComparisonExpression ("icmp slt", left, right);
	}
	static Expression* gt (Expression* left, Expression* right) {
		return new ComparisonExpression ("icmp sgt", left, right);
	}
	static Expression* le (Expression* left, Expression* right) {
		return new ComparisonExpression ("icmp sle", left, right);
	}
	static Expression* ge (Expression* left, Expression* right) {
		return new ComparisonExpression ("icmp sge", left, right);
	}
};

class And: public Expression {
	Expression* left;
	Expression* right;
public:
	And (Expression* left, Expression* right): left(left), right(right) {}
	writer::Value* insert (Writer& writer) override;
	const Type* get_type () override {
		return &Type::BOOL;
	}
	bool validate () override {
		return left->get_type() == &Type::BOOL && right->get_type() == &Type::BOOL;
	}
	static Expression* create (Expression* left, Expression* right) {
		return new And (left, right);
	}
};

class Or: public Expression {
	Expression* left;
	Expression* right;
public:
	Or (Expression* left, Expression* right): left(left), right(right) {}
	writer::Value* insert (Writer& writer) override;
	const Type* get_type () override {
		return &Type::BOOL;
	}
	bool validate () override {
		return left->get_type() == &Type::BOOL && right->get_type() == &Type::BOOL;
	}
	static Expression* create (Expression* left, Expression* right) {
		return new Or (left, right);
	}
};

class Node {
public:
	virtual void write (Writer&) = 0;
};

class ExpressionNode: public Node {
	Expression* expression;
public:
	ExpressionNode (Expression* expression): expression(expression) {}
	void write (Writer& writer) override {
		expression->insert (writer);
	}
};

class Return: public Node {
	Expression* expression;
public:
	Return (Expression* expression = nullptr): expression(expression) {}
	void write (Writer& writer) override;
};

class Block {
	std::vector<Node*> nodes;
	std::vector<Variable*> variables;
public:
	Block* parent;
	bool returns;
	Block (): parent(nullptr), returns(false) {}
	void add_node (Node* node) {
		nodes.push_back (node);
	}
	Variable* get_variable (const Substring& name) {
		for (Variable* variable: variables) {
			if (variable->get_name() == name) return variable;
		}
		if (parent) return parent->get_variable (name);
		return nullptr;
	}
	void add_variable (Variable* variable) {
		variables.push_back (variable);
	}
	void write (Writer& writer);
};

class If: public Node {
	Expression* condition;
public:
	Block* if_block;
	If (Expression* condition): condition(condition) {
		if_block = new Block ();
	}
	void write (Writer& writer) override;
};

class While: public Node {
	Expression* condition;
public:
	Block* block;
	While (Expression* condition): condition(condition) {
		block = new Block ();
	}
	void write (Writer& writer) override;
};

class FunctionDeclaration: public FunctionPrototype {
protected:
	Substring name;
	std::vector<Variable*> arguments;
	const Type* return_type;
public:
	FunctionDeclaration (const Substring& name): name(name), return_type(&Type::VOID) {}
	const Substring& get_name () const override {
		return name;
	}
	const Type* get_argument (int i) const override {
		if (i < arguments.size()) return arguments[i]->get_type();
		else return nullptr;
	}
	void add_argument (Variable* argument) {
		arguments.push_back (argument);
	}
	void set_return_type (const Type* return_type) {
		this->return_type = return_type;
	}
	const Type* get_return_type () const {
		return return_type;
	}
	//void write (Writer& writer);
};

class Function: public FunctionDeclaration {
	std::vector<Variable*> variables;
public:
	Block* block;
	Function (const Substring& name): FunctionDeclaration(name) {
		block = new Block ();
	}
	void add_argument (const Substring& name, const Type* type) {
		Variable* argument = new Variable (name, type);
		add_variable (argument);
		block->add_variable (argument);
		arguments.push_back (argument);
	}
	void add_variable (Variable* variable) {
		variable->set_n (variables.size());
		variables.push_back (variable);
	}
	void write (Writer& writer);
};

class Call: public Expression, public FunctionPrototype {
	Substring name;
	std::vector<Expression*> arguments;
	const Type* return_type;
public:
	Call (const Substring& name): name(name) {}
	const Substring& get_name () const override {
		return name;
	}
	const Type* get_argument (int i) const override {
		if (i < arguments.size()) return arguments[i]->get_type();
		else return nullptr;
	}
	void add_argument (Expression* argument) {
		arguments.push_back (argument);
	}
	void set_return_type (const Type* type) {
		return_type = type;
	}
	writer::Value* insert (Writer& writer);
	const Type* get_type () override {
		return return_type;
	}
};

class Class: public Type {
	Substring name;
	std::vector<Variable*> attributes;
	std::vector<Expression*> default_values;
public:
	Class (const Substring& name): name(name) {}
	Substring get_name () const override {
		return name;
	}
	void add_attribute (const Substring& name, Expression* value) {
		Variable* attribute = new Variable (name, value->get_type());
		attribute->set_n (attributes.size());
		attributes.push_back (attribute);
		default_values.push_back (value);
	}
	Variable* get_attribute (const Substring& name) const {
		for (Variable* attribute: attributes) {
			if (attribute->get_name() == name) return attribute;
		}
		return nullptr;
	}
	const std::vector<Variable*>& get_attributes () const {
		return attributes;
	}
	const std::vector<Expression*>& get_default_values () const {
		return default_values;
	}
	const Class* get_class () const override {
		return this;
	}
};

class Instantiation: public Expression {
	Class* _class;
	std::vector<Expression*> attribute_values;
public:
	Instantiation (Class* _class): _class(_class), attribute_values(_class->get_default_values()) {}
	void set_attribute_value (Variable* attribute, Expression* value) {
		attribute_values[attribute->get_n()] = value;
	}
	writer::Value* insert (Writer& writer) override;
	const Type* get_type () override {
		return _class;
	}
};

class AttributeAccess: public Expression {
	Expression* expression;
	Substring name;
public:
	AttributeAccess (Expression* expression, const Substring& name): expression(expression), name(name) {}
	writer::Value* insert (Writer& writer) override;
	bool has_address () override { return true; }
	writer::Value* insert_address (Writer& writer) override;
	const Type* get_type () override {
		return expression->get_type()->get_class()->get_attribute(name)->get_type();
	}
};

class Program {
	std::vector<FunctionDeclaration*> function_declarations;
	std::vector<Function*> functions;
	std::vector<Class*> classes;
public:
	void add_function_declaration (FunctionDeclaration* function_declaration) {
		function_declarations.push_back (function_declaration);
	}
	void add_function (Function* function) {
		functions.push_back (function);
	}
	const Type* get_return_type (const FunctionPrototype* function) {
		for (FunctionDeclaration* existing_function: function_declarations) {
			if (*existing_function == *function) return existing_function->get_return_type();
		}
		for (Function* existing_function: functions) {
			if (*existing_function == *function) return existing_function->get_return_type();
		}
		return nullptr;
	}
	void add_class (Class* _class) {
		classes.push_back (_class);
	}
	Class* get_class (const Substring& name) {
		for (Class* _class: classes) {
			if (_class->get_name() == name) return _class;
		}
		return nullptr;
	}
	void write (Writer& writer);
};

}
