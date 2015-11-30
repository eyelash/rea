/*

Copyright (c) 2015, Elias Aebi
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
#include <set>

class Cursor;
class Writer;

class Node {
public:
	virtual ~Node() = default;
	virtual void write (Writer&) = 0;
};

class Type {
	
};

class Expression: public Node {
public:
	virtual void evaluate (Writer&) = 0;
	virtual void insert (Writer&) = 0;
	virtual void write (Writer& w) {
		evaluate (w);
	}
	static Expression* parse (Cursor& cursor, int level = 0);
};

class Number: public Expression {
	int n;
public:
	Number (int n): n(n) {}
	void evaluate (Writer&) {}
	void insert (Writer& w);
	static Number* parse (Cursor& cursor);
};

class Variable: public Expression {
	Substring name;
	int n;
public:
	Variable (const Substring& name): name(name) {}
	void evaluate (Writer& writer);
	void insert (Writer& writer);
};

class Call: public Expression {
	Substring name;
	std::vector<Expression*> arguments;
	int n;
public:
	Call (const Substring& name): name(name) {}
	void append_argument (Expression* argument) {
		arguments.push_back (argument);
	}
	void evaluate (Writer& writer);
	void insert (Writer& writer);
};

class BinaryExpression: public Expression {
	const char* instruction;
	Expression* left;
	Expression* right;
	int n;
public:
	BinaryExpression (const char* instruction, Expression* left, Expression* right): instruction(instruction), left(left), right(right) {}
	void evaluate (Writer& writer);
	void insert (Writer& writer);
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
	static Expression* eq (Expression* left, Expression* right) {
		return new BinaryExpression ("icmp eq", left, right);
	}
	static Expression* ne (Expression* left, Expression* right) {
		return new BinaryExpression ("icmp ne", left, right);
	}
	static Expression* lt (Expression* left, Expression* right) {
		return new BinaryExpression ("icmp slt", left, right);
	}
	static Expression* gt (Expression* left, Expression* right) {
		return new BinaryExpression ("icmp sgt", left, right);
	}
	static Expression* le (Expression* left, Expression* right) {
		return new BinaryExpression ("icmp sle", left, right);
	}
	static Expression* ge (Expression* left, Expression* right) {
		return new BinaryExpression ("icmp sge", left, right);
	}
};

class Assignment: public Node {
	Substring name;
	Expression* expression;
public:
	Assignment (const Substring& name, Expression* expression): name(name), expression(expression) {}
	void write (Writer& writer);
	static Assignment* parse (Cursor& cursor);
};

class If: public Node {
	Expression* condition;
	std::vector<Node*> nodes;
public:
	If (Expression* condition): condition(condition) {}
	void append_node (Node* node) {
		nodes.push_back (node);
	}
	void write (Writer& writer);
	static If* parse (Cursor& cursor);
};

class While: public Node {
	Expression* condition;
	std::vector<Node*> nodes;
public:
	While (Expression* condition): condition(condition) {}
	void append_node (Node* node) {
		nodes.push_back (node);
	}
	void write (Writer& writer);
	static While* parse (Cursor& cursor);
};

class Function: public Node {
	Substring name;
	std::vector<Substring> arguments;
	std::set<Substring> variables;
	std::vector<Node*> nodes;
public:
	Function (const Substring& name): name(name) {}
	void append_argument (const Substring& argument) {
		arguments.push_back (argument);
	}
	void append_node (Node* node) {
		nodes.push_back (node);
	}
	void add_variable (const Substring& variable) {
		variables.insert (variable);
	}
	void write (Writer& writer);
	static Function* parse (Cursor& cursor);
};

class Program: public Node {
	std::vector<Node*> nodes;
public:
	void append_node (Node* node) {
		nodes.push_back (node);
	}
	void write (Writer& writer);
	static Program* parse (Cursor& cursor);
};
