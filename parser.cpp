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

Number* NumberParser::parse (Cursor& cursor) {
	int n = 0;
	while (cursor[0].is_numeric()) {
		n *= 10;
		n += *cursor - '0';
		++cursor;
	}
	return new Number (n);
}

Substring IdentifierParser::parse (Cursor& cursor) {
	if (!cursor[0].is_alphabetic()) cursor.error ("expected alphabetic character");
	int i = 0;
	while (cursor[i].is_alphanumeric()) {
		++i;
	}
	Substring result = cursor.get_substring (i);
	cursor.advance (i);
	return result;
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

Expression* ExpressionParser::parse (Cursor& cursor, int level) {
	if (level == 3) {
		if (cursor.starts_with("(")) {
			cursor.skip_whitespace ();
			Expression* expression = parse (cursor);
			cursor.skip_whitespace ();
			cursor.expect (")");
			return expression;
		}
		else if (cursor[0].is_numeric()) {
			// number
			return NumberParser(this).parse (cursor);
		}
		else if (cursor[0].is_alphabetic()) {
			Substring identifier = IdentifierParser(this).parse (cursor);
			Variable* variable = Parser::get_variable (identifier);
			if (variable) {
				return variable;
			}
			Function* function = Parser::get_function (identifier);
			if (function) {
				cursor.skip_whitespace ();
				cursor.expect ("(");
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
			cursor.error ([&](FILE* f){
				fputs ("undefined identifier '", f);
				identifier.write (f);
				fputs ("'", f);
			});
			return nullptr;
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

Assignment* AssignmentParser::parse (Cursor& cursor) {
	cursor.expect ("var");
	cursor.skip_whitespace ();
	Substring name = IdentifierParser(this).parse (cursor);
	cursor.skip_whitespace ();
	cursor.expect ("=");
	cursor.skip_whitespace ();
	Expression* expression = ExpressionParser(this).parse (cursor);
	Variable* variable = Parser::get_variable (name);
	if (!variable)
		variable = Parser::add_variable (name);
	return new Assignment (variable, expression);
}

Node* LineParser::parse (Cursor& cursor) {
	if (cursor.starts_with("var", false)) {
		return AssignmentParser(this).parse (cursor);
	}
	else if (cursor.starts_with("if", false)) {
		return IfParser(this).parse (cursor);
	}
	else if (cursor.starts_with("while", false)) {
		return WhileParser(this).parse (cursor);
	}
	else {
		return new ExpressionNode (ExpressionParser(this).parse (cursor));
	}
}

std::vector<Node*> BlockParser::parse (Cursor& cursor) {
	std::vector<Node*> result;
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}') {
		Node* node = LineParser(this).parse (cursor);
		result.push_back (node);
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	return result;
}

If* IfParser::parse (Cursor& cursor) {
	cursor.expect ("if");
	cursor.skip_whitespace ();
	Expression* condition = ExpressionParser(this).parse (cursor);
	If* result = new If (condition);
	cursor.skip_whitespace ();
	std::vector<Node*> nodes = BlockParser(this).parse (cursor);
	result->append_nodes (nodes);
	return result;
}

While* WhileParser::parse (Cursor& cursor) {
	cursor.expect ("while");
	cursor.skip_whitespace ();
	Expression* condition = ExpressionParser(this).parse (cursor);
	While* result = new While (condition);
	cursor.skip_whitespace ();
	std::vector<Node*> nodes = BlockParser(this).parse (cursor);
	result->append_nodes (nodes);
	return result;
}

Function* FunctionParser::parse (Cursor& cursor) {
	cursor.expect ("func");
	cursor.skip_whitespace ();
	Substring name = IdentifierParser(this).parse (cursor);
	function = new Function (name);
	cursor.skip_whitespace ();
	BlockParser block_parser (this);
	if (cursor.starts_with("(")) {
		cursor.skip_whitespace ();
		while (*cursor != ')') {
			Substring argument_name = IdentifierParser(this).parse (cursor);
			Variable* argument = block_parser.add_variable (argument_name);
			function->append_argument (argument);
			cursor.skip_whitespace ();
		}
		cursor.expect (")");
		cursor.skip_whitespace ();
	}
	std::vector<Node*> nodes = block_parser.parse (cursor);
	function->append_nodes (nodes);
	Parser::add_function (function);
	return function;
}

Program* ProgramParser::parse (Cursor& cursor) {
	program = new Program ();
	add_function (new Function("print"));
	cursor.skip_whitespace ();
	while (*cursor != '\0') {
		Node* node = FunctionParser(this).parse (cursor);
		program->append_node (node);
		cursor.skip_whitespace ();
	}
	return program;
}
