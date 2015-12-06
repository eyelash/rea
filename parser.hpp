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
#include <cstdio>
#include <map>

#define CSI "\e["
#define RESET CSI "m"
#define BOLD CSI "1" "m"
#define RED CSI "31" "m"
#define YELLOW CSI "33" "m"

class Cursor {
	const char* string;
	int position;
	int line;
	void print_line (FILE* stream) {
		for (int i = 0; string[i] != '\n' && string[i] != '\0'; i++)
			fputc (string[i], stream);
		fputc ('\n', stream);
	}
public:
	Cursor(const char* string): string(string), position(0), line(1) {}
	template <class F> void error (const F& functor) {
		fprintf (stderr, BOLD "line %d: " RED "error: " RESET BOLD, line);
		functor (stderr);
		fputs (RESET "\n", stderr);
		print_line (stderr);
		for (int i = 0; i < position; i++) fputc (' ', stderr);
		fputs (BOLD "^" RESET "\n", stderr);
		exit (EXIT_FAILURE);
	}
	void error (const char* message) {
		error ([&](FILE* f){ fputs (message, f); });
	}
	void advance () {
		if (string[position] == '\n') {
			++line;
			string += position + 1;
			position = 0;
		}
		else {
			++position;
		}
	}
	void advance (int n) {
		for (int i = 0; i < n; i++)
			advance ();
	}
	void skip_whitespace () {
		while (true) {
			if (Character(string[position]).is_whitespace()) {
				advance ();
			}
			else if (starts_with("//")) {
				while (string[position] != '\n' && string[position] != '\0')
					advance ();
			}
			else if (starts_with("/*")) {
				while (!starts_with("*/") && string[position] != '\0')
					advance ();
			}
			else {
				break;
			}
		}
	}
	bool starts_with (const char* s, bool adv = true) {
		for (int i = 0;; i++) {
			if (s[i] == '\0') {
				if (adv) advance (i);
				return true;
			}
			if (s[i] != string[position+i])
				return false;
		}
	}
	bool expect (const char* s) {
		if (!starts_with(s)) {
			error ([&](FILE* f){ fprintf (f, "expected '%s'", s); });
			return false;
		}
		else
			return true;
	}
	void expect_end () {
		
	}
	Substring get_substring (int length) const {
		return Substring (string + position, length);
	}
	Character operator [] (int i) const {
		return string[position + i];
	}
	Cursor& operator ++ () {
		advance ();
		return *this;
	}
	Character operator * () const {
		return string[position];
	}
};

class Parser {
	Parser* parent;
public:
	Parser (Parser* parent = nullptr): parent(parent) {}
	Parser* get_parent () { return parent; }
	virtual Variable* get_variable (const Substring& name) {
		if (parent) return parent->get_variable (name);
		return nullptr;
	}
	virtual Variable* add_variable_to_function (const Type* type) {
		if (parent) return parent->add_variable_to_function (type);
		return nullptr;
	}
	virtual void add_variable_to_scope (const Substring& name, Variable* variable) {
		if (parent) parent->add_variable_to_scope (name, variable);
	}
	Variable* add_variable (const Substring& name, const Type* type) {
		Variable* variable = add_variable_to_function (type);
		add_variable_to_scope (name, variable);
		return variable;
	}
	virtual Function* get_function (const Substring& name) {
		if (parent) return parent->get_function (name);
		return nullptr;
	}
	virtual void add_function (Function* function) {
		if (parent) parent->add_function (function);
	}
};

class TypeParser: public Parser {
public:
	TypeParser (Parser* parent = nullptr): Parser(parent) {}
	Type* parse (Cursor& cursor, bool allow_void);
};

class NumberParser: public Parser {
public:
	NumberParser (Parser* parent = nullptr): Parser(parent) {}
	Number* parse (Cursor& cursor);
};

class IdentifierParser: public Parser {
public:
	IdentifierParser (Parser* parent = nullptr): Parser(parent) {}
	Substring parse (Cursor& cursor);
};

class ExpressionParser: public Parser {
public:
	ExpressionParser (Parser* parent = nullptr): Parser(parent) {}
	Expression* parse (Cursor& cursor, int level = 0);
};

class AssignmentParser: public Parser {
public:
	AssignmentParser (Parser* parent = nullptr): Parser(parent) {}
	Assignment* parse (Cursor& cursor);
};

class LineParser: public Parser {
public:
	LineParser (Parser* parent = nullptr): Parser(parent) {}
	Node* parse (Cursor& cursor);
};

class BlockParser: public Parser {
	std::map<Substring, Variable*> variables;
public:
	BlockParser (Parser* parent = nullptr): Parser(parent) {}
	std::vector<Node*> parse (Cursor& cursor);
	Variable* get_variable (const Substring& name) override {
		auto i = variables.find (name);
		if (i != variables.end())
			return i->second;
		return Parser::get_variable (name);
	}
	void add_variable_to_scope (const Substring& name, Variable* variable) override {
		variables[name] = variable;
	}
};

class IfParser: public Parser {
public:
	IfParser (Parser* parent = nullptr): Parser(parent) {}
	If* parse (Cursor& cursor);
};

class WhileParser: public Parser {
public:
	WhileParser (Parser* parent = nullptr): Parser(parent) {}
	While* parse (Cursor& cursor);
};

class FunctionParser: public Parser {
	Function* function;
public:
	FunctionParser (Parser* parent = nullptr): Parser(parent) {}
	Function* parse (Cursor& cursor);
	Variable* add_variable_to_function (const Type* type) override {
		return function->add_variable (type);
	}
};

class ProgramParser: public Parser {
	Program* program;
	std::map<Substring, Function*> functions;
public:
	ProgramParser (Parser* parent = nullptr): Parser(parent) {}
	Program* parse (Cursor& cursor);
	Function* get_function (const Substring& name) override {
		auto i = functions.find (name);
		if (i != functions.end())
			return i->second;
		return Parser::get_function (name);
	}
	void add_function (Function* function) override {
		functions[function->get_name()] = function;
	}
};
