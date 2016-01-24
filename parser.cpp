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

const Type* Parser::parse_type (bool allow_void) {
	if (allow_void && cursor.starts_with("Void"))
		return &Type::VOID;
	if (cursor.starts_with("Bool"))
		return &Type::BOOL;
	if (cursor.starts_with("Int"))
		return &Type::INT;
	Substring identifier = parse_identifier ();
	const Type* type = context.get_class (identifier);
	if (type) return type;
	cursor.error ("unknown type");
	return nullptr;
}

Number* Parser::parse_number () {
	int n = 0;
	while (cursor[0].is_numeric()) {
		n *= 10;
		n += *cursor - '0';
		++cursor;
	}
	return new Number (n);
}

Substring Parser::parse_identifier () {
	if (!(cursor[0].is_alphabetic() || *cursor == '_')) cursor.error ("expected alphabetic character");
	int i = 0;
	while (cursor[i].is_alphanumeric() || cursor[i] == '_') {
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
typedef ComparisonExpression CE;
static Operator operators[][7] = {
	{{"=", Assignment::create}, NULL},
	{{"==", CE::eq}, {"!=", CE::ne}, {"<=", CE::le}, {">=", CE::ge}, {"<", CE::lt}, {">", CE::gt}, NULL},
	{{"+", BE::add}, {"-", BE::sub}, NULL},
	{{"*", BE::mul}, {"/", BE::div}, {"%", BE::mod}, NULL}
};

Expression* Parser::parse_expression_last () {
	if (cursor.starts_with("(")) {
		cursor.skip_whitespace ();
		Expression* expression = parse_expression ();
		cursor.skip_whitespace ();
		cursor.expect (")");
		return expression;
	}
	else if (cursor.starts_with("false")) {
		return new BooleanLiteral (false);
	}
	else if (cursor.starts_with("true")) {
		return new BooleanLiteral (true);
	}
	else if (cursor[0].is_numeric()) {
		return parse_number ();
	}
	else if (cursor[0].is_alphabetic() || *cursor == '_') {
		Substring identifier = parse_identifier ();
		
		// variable
		Variable* variable = context.get_variable (identifier);
		if (variable) {
			return variable;
		}
		
		// class instantiation
		Class* _class = context.get_class (identifier);
		if (_class) {
			cursor.skip_whitespace ();
			cursor.expect ("{");
			cursor.skip_whitespace ();
			cursor.expect ("}");
			return new Instantiation (_class);
		}
		
		// function call
		cursor.skip_whitespace ();
		cursor.expect ("(");
		Call* call = new Call (identifier);
		cursor.skip_whitespace ();
		while (*cursor != ')') {
			Expression* argument = parse_expression ();
			call->add_argument (argument);
			cursor.skip_whitespace ();
		}
		const Type* return_type = context.get_return_type (call);
		if (!return_type) cursor.error ("invalid call");
		call->set_return_type (return_type);
		cursor.expect (")");
		return call;
	}
	else {
		cursor.error ("unexpected character");
		return nullptr;
	}
}
Expression* Parser::parse_expression (int level) {
	if (level == 4) {
		Expression* expression = parse_expression_last ();
		cursor.skip_whitespace ();
		while (cursor.starts_with(".")) {
			cursor.skip_whitespace ();
			Substring identifier = parse_identifier ();
			const Class* _class = expression->get_type()->get_class();
			if (_class && _class->get_attribute(identifier)) {
				expression = new AttributeAccess (expression, identifier);
			}
			else {
				// method call
				Call* call = new Call (identifier);
				call->add_argument (expression);
				cursor.skip_whitespace ();
				cursor.expect ("(");
				while (*cursor != ')') {
					Expression* argument = parse_expression ();
					call->add_argument (argument);
					cursor.skip_whitespace ();
				}
				const Type* return_type = context.get_return_type (call);
				if (!return_type) cursor.error ("invalid method call");
				call->set_return_type (return_type);
				cursor.expect (")");
				expression = call;
			}
			cursor.skip_whitespace ();
		}
		return expression;
	}
	Expression* left = parse_expression (level + 1);
	cursor.skip_whitespace ();
	bool match = true;
	while (match) {
		match = false;
		for (Operator* op = operators[level]; op->identifier && !match; op++) {
			if (cursor.starts_with(op->identifier)) {
				cursor.skip_whitespace ();
				Expression* right = parse_expression (level + 1);
				if (op->create) left = op->create (left, right);
				if (!left->validate()) cursor.error ([&](FILE* f){
					fprintf (f, "invalid operands for operator '%s'", op->identifier);
				});
				match = true;
			}
		}
	}
	return left;
}

Node* Parser::parse_variable_definition () {
	cursor.expect ("var");
	cursor.skip_whitespace ();
	Substring name = parse_identifier ();
	if (context.get_variable(name)) cursor.error ("variable already defined");
	cursor.skip_whitespace ();
	cursor.expect ("=");
	cursor.skip_whitespace ();
	Expression* expression = parse_expression ();
	if (expression->get_type() == &Type::VOID) cursor.error ("variables of type Void are not allowed");
	Expression* variable = context.add_variable (name, expression->get_type());
	Assignment* assignment = new Assignment (variable, expression);
	return new ExpressionNode (assignment);
}

Node* Parser::parse_line () {
	if (cursor.starts_with("var", false)) {
		return parse_variable_definition ();
	}
	else if (cursor.starts_with("if", false)) {
		return parse_if ();
	}
	else if (cursor.starts_with("while", false)) {
		return parse_while ();
	}
	else if (cursor.starts_with("return")) {
		const Type* return_type = context.get_return_type ();
		Expression* expression = nullptr;
		if (return_type != &Type::VOID) {
			cursor.skip_whitespace ();
			expression = parse_expression ();
			if (expression->get_type() != return_type)
				cursor.error ("invalid return type");
		}
		context.set_returned ();
		return new Return (expression);
	}
	else {
		return new ExpressionNode (parse_expression ());
	}
}

void Parser::parse_block (Block* block) {
	block->parent = context.block;
	context.block = block;
	
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}' && !block->returns && *cursor != '\0') {
		Node* node = parse_line ();
		block->add_node (node);
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	
	context.block = block->parent;
}

If* Parser::parse_if () {
	cursor.expect ("if");
	cursor.skip_whitespace ();
	Expression* condition = parse_expression ();
	if (condition->get_type() != &Type::BOOL) cursor.error ("condition must be of type Bool");
	If* result = new If (condition);
	cursor.skip_whitespace ();
	parse_block (result->if_block);
	return result;
}

While* Parser::parse_while () {
	cursor.expect ("while");
	cursor.skip_whitespace ();
	Expression* condition = parse_expression ();
	if (condition->get_type() != &Type::BOOL) cursor.error ("condition must be of type Bool");
	While* result = new While (condition);
	cursor.skip_whitespace ();
	parse_block (result->block);
	return result;
}

void Parser::parse_function () {
	Context previous_context = context;
	
	cursor.expect ("func");
	cursor.skip_whitespace ();
	
	// name
	Substring name = parse_identifier ();
	Function* function = new Function (name);
	context.function = function;
	context._class = nullptr;
	cursor.skip_whitespace ();
	
	// argument list
	if (previous_context._class) {
		function->add_argument ("this", previous_context._class);
	}
	cursor.expect("(");
	cursor.skip_whitespace ();
	while (*cursor != ')') {
		Substring argument_name = parse_identifier ();
		cursor.skip_whitespace ();
		cursor.expect (":");
		cursor.skip_whitespace ();
		const Type* argument_type = parse_type (false);
		// TODO: report error for duplicate argument names
		function->add_argument (argument_name, argument_type);
		cursor.skip_whitespace ();
	}
	cursor.expect (")");
	cursor.skip_whitespace ();
	
	// return type
	if (cursor.starts_with(":")) {
		cursor.skip_whitespace ();
		const Type* return_type = parse_type (true);
		function->set_return_type (return_type);
		cursor.skip_whitespace ();
	}
	
	if (context.get_return_type(function)) cursor.error ("function already defined");
	context.add_function (function);
	
	// code block
	parse_block (function->block);
	if (function->get_return_type() != &Type::VOID && !function->block->returns)
		cursor.error ("missing return statement");
	
	context = previous_context;
}

void Parser::parse_class () {
	Context previous_context = context;
	
	cursor.expect ("class");
	cursor.skip_whitespace ();
	Substring name = parse_identifier ();
	Class* _class = new Class (name);
	context.add_class (_class);
	context.add_function (_class->get_constructor());
	context._class = _class;
	context.function = nullptr;
	cursor.skip_whitespace ();
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}' && *cursor != '\0') {
		if (cursor.starts_with("var", false)) {
			Node* node = parse_variable_definition ();
			_class->get_constructor()->block->add_node (node);
		}
		else if (cursor.starts_with("func", false)) {
			parse_function ();
		}
		else {
			cursor.error ("unexpected character");
		}
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	
	context = previous_context;
}

static FunctionDeclaration* create_function (const char* name, std::initializer_list<const Type*> arguments, const Type* return_type = &Type::VOID) {
	FunctionDeclaration* function = new FunctionDeclaration (name);
	for (const Type* type: arguments)
		function->append_argument (new Variable ("", type));
	function->set_return_type (return_type);
	return function;
}

Program* Parser::parse_program () {
	Program* program = new Program ();
	context.program = program;
	program->add_function_declaration (create_function("print", {&Type::INT}));
	cursor.skip_whitespace ();
	while (*cursor != '\0') {
		if (cursor.starts_with("func", false)) {
			parse_function ();
		}
		else if (cursor.starts_with("class", false)) {
			parse_class ();
		}
		else {
			cursor.error ("unexpected character");
		}
		cursor.skip_whitespace ();
	}
	return program;
}
