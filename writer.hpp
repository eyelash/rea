/*

Copyright (c) 2015, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "foundation.hpp"
#include <cstdio>
#include <vector>

#define INDENT "  "

class Writer;

class Printable {
public:
	virtual void print (Writer&) = 0;
};
class ConstPrintable {
public:
	virtual void print (Writer&) const = 0;
};

class Writer {
	FILE* file;
	int value_counter;
	int label_counter;
public:
	Writer (): file(stdout), value_counter(0), label_counter(0) {}
	void write (const char* str) {
		fputs (str, file);
	}
	void write (int n) {
		fprintf (file, "%d", n);
	}
	void write (const Substring& substring) {
		substring.write (file);
	}
	void write (Printable* printable) {
		printable->print (*this);
	}
	void write (const ConstPrintable* printable) {
		printable->print (*this);
	}
	void write (const ConstPrintable& printable) {
		printable.print (*this);
	}
	template <class T0, class... T> void write (const char* s, const T0& v0, const T&... v) {
		while (true) {
			if (*s == '\0') return;
			if (*s == '%') {
				++s;
				if (*s != '%') break;
			}
			fputc (*s, file);
			++s;
		}
		write (v0);
		write (s, v...);
	}
	int get_next_value () {
		++value_counter;
		return value_counter;
	}
	int get_next_label () {
		++label_counter;
		return label_counter;
	}
	void reset () {
		value_counter = 0;
		label_counter = 0;
	}
};

#include "type.hpp"

class Expression: public Printable {
public:
	virtual void evaluate (Writer&) = 0;
	virtual void insert (Writer&) = 0;
	virtual bool has_address () { return false; }
	virtual void evaluate_address (Writer&) {}
	virtual void insert_address (Writer&) {}
	virtual const Type* get_type () = 0;
	virtual bool validate () { return true; }
	void print (Writer& writer) override {
		insert (writer);
	}
	class Address: public ConstPrintable {
		Expression* expression;
	public:
		Address (Expression* expression): expression(expression) {}
		void print (Writer& writer) const override { expression->insert_address(writer); }
	};
	Address get_address () {
		return Address (this);
	}
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
	void insert_mangled_name (Writer& writer);
	class MangledName: public ConstPrintable {
		FunctionPrototype* prototype;
	public:
		MangledName (FunctionPrototype* prototype): prototype(prototype) {}
		void print (Writer& writer) const {
			prototype->insert_mangled_name (writer);
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
	void evaluate (Writer&) override {}
	void insert (Writer& w) override;
	const Type* get_type () override {
		return &Type::INT;
	}
};

class BooleanLiteral: public Expression {
	bool value;
public:
	BooleanLiteral (bool value): value(value) {}
	void evaluate (Writer&) override {}
	void insert (Writer& writer) override;
	const Type* get_type () override {
		return &Type::BOOL;
	}
};

class Variable: public Expression {
	Substring name;
	const Type* type;
	int n;
	int value;
public:
	Variable (const Substring& name, const Type* type): name(name), type(type) {}
	const Substring& get_name () const {
		return name;
	}
	void set_n (int n) { this->n = n; }
	int get_n () const { return n; }
	void evaluate (Writer& writer) override;
	void insert (Writer& writer) override;
	bool has_address () override { return true; }
	void insert_address (Writer& writer);
	const Type* get_type () override {
		return type;
	}
};

class Assignment: public Expression {
	Expression* left;
	Expression* right;
public:
	Assignment (Expression* left, Expression* right): left(left), right(right) {}
	void evaluate (Writer& writer) override;
	void insert (Writer& writer) override;
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
	int value;
public:
	BinaryExpression (const char* instruction, Expression* left, Expression* right): instruction(instruction), left(left), right(right) {}
	void evaluate (Writer& writer) override;
	void insert (Writer& writer) override;
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

class Node: public Printable {
public:
	virtual void write (Writer&) = 0;
	void print (Writer& writer) override {
		write (writer);
	}
};

class ExpressionNode: public Node {
	Expression* expression;
public:
	ExpressionNode (Expression* expression): expression(expression) {}
	void write (Writer& writer) override {
		expression->evaluate (writer);
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
	void write (Writer& writer);
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
	int value;
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
	void evaluate (Writer& writer);
	void insert (Writer& writer);
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
	const std::vector<Expression*>& get_default_values () const {
		return default_values;
	}
	void write (Writer& writer);
	void insert (Writer& writer) const override;
	const Class* get_class () const override {
		return this;
	}
};

class Instantiation: public Expression {
	Class* _class;
	std::vector<Expression*> attribute_values;
	int value;
public:
	Instantiation (Class* _class): _class(_class), attribute_values(_class->get_default_values()) {}
	void set_attribute_value (Variable* attribute, Expression* value) {
		attribute_values[attribute->get_n()] = value;
	}
	void evaluate (Writer& writer) override;
	void insert (Writer& writer) override;
	const Type* get_type () override {
		return _class;
	}
};

class AttributeAccess: public Expression {
	Expression* expression;
	Substring name;
	int address;
	int value;
public:
	AttributeAccess (Expression* expression, const Substring& name): expression(expression), name(name) {}
	void evaluate (Writer& writer) override;
	void insert (Writer& writer) override;
	bool has_address () override { return true; }
	void evaluate_address (Writer& writer) override;
	void insert_address (Writer& writer) override;
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
