/*

Copyright (c) 2015, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "parser.hpp"

Number* Number::parse (Cursor& cursor) {
	int n = 0;
	while (isdigit(*cursor)) {
		n *= 10;
		n += *cursor - '0';
		++cursor;
	}
	return new Number(n);
}

struct Operator {
	const char* identifier;
	Expression* (*create) (Expression*, Expression*);
};

typedef BinaryExpression BE;
static Operator operators[][7] = {
	{{"==", BE::eq}, {"!=", BE::ne}, {"<=", BE::le}, {">=", BE::ge}, {"<", BE::lt}, {">", BE::gt}, NULL},
	{{"+", BE::add}, {"-", BE::sub}, NULL},
	{{"*", BE::mul}, {"/", BE::div}, {"%", BE::mod}, NULL}
};

static Substring parse_identifier(Cursor& cursor) {
	int i = 0;
	while (isalnum(cursor[i])) {
		++i;
	}
	Substring result = cursor.get_substring (i);
	cursor.advance (i);
	return result;
}

Expression* Expression::parse (Cursor& cursor, int level) {
	if (level == 3) {
		if (cursor.starts_with("(")) {
			cursor.skip_whitespace ();
			Expression* expression = parse (cursor);
			cursor.skip_whitespace ();
			cursor.expect (")");
			return expression;
		}
		else if (isdigit(*cursor)) {
			// number
			return Number::parse (cursor);
		}
		else if (isalnum(*cursor)) {
			Substring identifier = parse_identifier (cursor);
			cursor.skip_whitespace ();
			if (cursor.starts_with("(")) {
				// function call
				Call* call = new Call (identifier);
				cursor.skip_whitespace ();
				while (*cursor != ')') {
					Expression* argument = parse (cursor);
					call->append_argument (argument);
					cursor.skip_whitespace ();
				}
				cursor.expect (")");
				return call;
			}
			else {
				// variable
				return new Variable (identifier);
			}
		}
		else {
			cursor.error ("unexpected character");
			return nullptr;
		}
	}
	Expression* left = parse (cursor, level + 1);
	cursor.skip_whitespace ();
	bool match = true;
	while (match) {
		match = false;
		for (Operator* op = operators[level]; op->identifier && !match; op++) {
			if (cursor.starts_with(op->identifier)) {
				cursor.skip_whitespace ();
				Expression* right = parse (cursor, level + 1);
				if (op->create) left = op->create (left, right);
				match = true;
			}
		}
	}
	return left;
}

Assignment* Assignment::parse (Cursor& cursor) {
	cursor.expect ("var");
	cursor.skip_whitespace ();
	Substring name = parse_identifier (cursor);
	cursor.get_function()->add_variable (name);
	cursor.skip_whitespace ();
	cursor.expect ("=");
	cursor.skip_whitespace ();
	Expression* expression = Expression::parse (cursor);
	return new Assignment (name, expression);
}

static Node* parse_line (Cursor& cursor) {
	if (cursor.starts_with("var", false)) {
		return Assignment::parse (cursor);
	}
	else if (cursor.starts_with("if", false)) {
		return If::parse (cursor);
	}
	else if (cursor.starts_with("while", false)) {
		return While::parse (cursor);
	}
	else {
		return Expression::parse (cursor);
	}
}

If* If::parse (Cursor& cursor) {
	cursor.expect ("if");
	cursor.skip_whitespace ();
	Expression* condition = Expression::parse (cursor);
	If* result = new If (condition);
	cursor.skip_whitespace ();
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}') {
		Node* node = parse_line (cursor);
		result->append_node (node);
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	return result;
}

While* While::parse (Cursor& cursor) {
	cursor.expect ("while");
	cursor.skip_whitespace ();
	Expression* condition = Expression::parse (cursor);
	While* result = new While (condition);
	cursor.skip_whitespace ();
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}') {
		Node* node = parse_line (cursor);
		result->append_node (node);
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	return result;
}

Function* Function::parse (Cursor& cursor) {
	cursor.expect ("func");
	cursor.skip_whitespace ();
	Substring name = parse_identifier (cursor);
	Function* function = new Function(name);
	cursor.set_function (function);
	cursor.skip_whitespace ();
	if (cursor.starts_with("(")) {
		cursor.skip_whitespace ();
		while (*cursor != ')') {
			Substring argument = parse_identifier (cursor);
			function->append_argument (argument);
			cursor.skip_whitespace ();
		}
		cursor.expect (")");
		cursor.skip_whitespace ();
	}
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}') {
		Node* node = parse_line (cursor);
		function->append_node (node);
		cursor.skip_whitespace ();
	}
	++cursor;
	return function;
}

Program* Program::parse (Cursor& cursor) {
	Program* program = new Program ();
	cursor.skip_whitespace ();
	while (*cursor != '\0') {
		Node* node = Function::parse (cursor);
		program->append_node (node);
		cursor.skip_whitespace ();
	}
	return program;
}
