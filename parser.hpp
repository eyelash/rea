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

#define CSI "\e["
#define RESET CSI "m"
#define BOLD CSI "1" "m"
#define RED CSI "31" "m"
#define YELLOW CSI "33" "m"

class Cursor {
	const char* string;
	int position;
	int line;
	void print_position (FILE* stream) {
		for (int i = 0; string[i] != '\n' && string[i] != '\0'; i++)
			fputc (string[i], stream);
		fputc ('\n', stream);
		for (int i = 0; i < position; i++) {
			if (string[i] == '\t') fputc ('\t', stream);
			else fputc (' ', stream);
		}
		fputs (BOLD "^" RESET "\n", stream);
	}
	void print_message (const char* str) {
		fputs (str, stderr);
	}
	void print_message (const Substring& substring) {
		substring.write (stderr);
	}
	template <class T0, class... T> void print_message (const char* s, const T0& v0, const T&... v) {
		while (true) {
			if (*s == '\0') return;
			if (*s == '%') {
				++s;
				if (*s != '%') break;
			}
			fputc (*s, stderr);
			++s;
		}
		print_message (v0);
		print_message (s, v...);
	}
public:
	Cursor(const char* string): string(string), position(0), line(1) {}
	template <class... T> void error (const char* s, const T&... v) {
		fprintf (stderr, BOLD "line %d: " RED "error: " RESET BOLD, line);
		print_message (s, v...);
		fputs (RESET "\n", stderr);
		print_position (stderr);
		exit (EXIT_FAILURE);
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
			error ("expected '%'", s);
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

class Context {
public:
	Program* program;
	Class* _class;
	Function* function;
	Block* block;
public:
	Context (): program(nullptr), _class(nullptr), function(nullptr), block(nullptr) {}
	Class* get_class (const Substring& name) {
		return program->get_class (name);
	}
	void add_class (Class* _class) {
		program->add_class (_class);
	}
	const Type* get_return_type (const FunctionPrototype* function) {
		return program->get_return_type (function);
	}
	void add_function (Function* function) {
		program->add_function (function);
	}
	Variable* get_variable (const Substring& name) {
		if (_class) {
			return _class->get_attribute (name);
		}
		else if (function && block) {
			return block->get_variable (name);
		}
		return nullptr;
	}
	Expression* add_variable (const Substring& name, const Type* type) {
		Variable* variable = new Variable (name, type);
		if (function && block) {
			function->add_variable (variable);
			block->add_variable (variable);
			return variable;
		}
		return nullptr;
	}
	const Type* get_return_type () {
		return function->get_return_type ();
	}
	void set_returned () {
		block->returns = true;
	}
};

class Parser {
	Context context;
	Cursor& cursor;
public:
	Parser (Cursor& cursor): cursor(cursor) {}
	const Type* parse_type ();
	Number* parse_number ();
	Substring parse_identifier ();
	Expression* parse_expression (int level = 0);
	Expression* parse_expression_last ();
	Node* parse_variable_definition ();
	Node* parse_line ();
	void parse_block (Block* block);
	If* parse_if ();
	While* parse_while ();
	void parse_function ();
	void parse_class ();
	Program* parse_program ();
};
