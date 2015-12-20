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

const Type* TypeParser::parse (Cursor& cursor, bool allow_void) {
	if (allow_void && cursor.starts_with("Void"))
		return &Type::VOID;
	if (cursor.starts_with("Bool"))
		return &Type::BOOL;
	if (cursor.starts_with("Int"))
		return &Type::INT;
	Substring identifier = IdentifierParser(this).parse (cursor);
	const Type* type = get_class (identifier);
	if (type) return type;
	cursor.error ("unknown type");
	return nullptr;
}

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

Expression* ExpressionParser::parse_last (Cursor& cursor) {
	if (cursor.starts_with("(")) {
		cursor.skip_whitespace ();
		Expression* expression = parse (cursor);
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
		return NumberParser(this).parse (cursor);
	}
	else if (cursor[0].is_alphabetic() || *cursor == '_') {
		Substring identifier = IdentifierParser(this).parse (cursor);
		
		// variable
		Variable* variable = Parser::get_variable (identifier);
		if (variable) {
			return variable;
		}
		
		// function call
		FunctionDeclaration* function = Parser::get_function (identifier);
		if (function) {
			cursor.skip_whitespace ();
			cursor.expect ("(");
			Call* call = new Call (function);
			cursor.skip_whitespace ();
			while (*cursor != ')') {
				Expression* argument = parse (cursor);
				call->append_argument (argument);
				cursor.skip_whitespace ();
			}
			if (!call->validate()) cursor.error ("invalid arguments");
			cursor.expect (")");
			return call;
		}
		
		// class instantiation
		Class* _class = get_class (identifier);
		if (_class) {
			cursor.skip_whitespace ();
			cursor.expect ("{");
			cursor.skip_whitespace ();
			cursor.expect ("}");
			return new Instantiation (_class);
		}
		
		// error
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
Expression* ExpressionParser::parse (Cursor& cursor, int level) {
	if (level == 4) {
		Expression* expression = parse_last (cursor);
		// member access
		cursor.skip_whitespace ();
		while (cursor.starts_with(".")) {
			const Class* _class = expression->get_type()->get_class();
			if (!_class) cursor.error ("variable is no class type");
			cursor.skip_whitespace ();
			Substring identifier = IdentifierParser(this).parse (cursor);
			if (!_class->get_attribute(identifier))
				cursor.error ("invalid member");
			expression = new AttributeAccess (expression, identifier);
			cursor.skip_whitespace ();
		}
		return expression;
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
				if (!left->validate()) cursor.error ([&](FILE* f){
					fprintf (f, "invalid operands for operator '%s'", op->identifier);
				});
				match = true;
			}
		}
	}
	return left;
}

Node* VariableDefinitionParser::parse (Cursor& cursor) {
	cursor.expect ("var");
	cursor.skip_whitespace ();
	Substring name = IdentifierParser(this).parse (cursor);
	if (get_variable(name)) cursor.error ("variable already defined");
	cursor.skip_whitespace ();
	cursor.expect ("=");
	cursor.skip_whitespace ();
	Expression* expression = ExpressionParser(this).parse (cursor);
	if (expression->get_type() == &Type::VOID) cursor.error ("variables of type Void are not allowed");
	Variable* variable = add_variable (name, expression->get_type());
	Assignment* assignment = new Assignment (variable, expression);
	return new ExpressionNode (assignment);
}

Node* LineParser::parse (Cursor& cursor) {
	if (cursor.starts_with("var", false)) {
		return VariableDefinitionParser(this).parse (cursor);
	}
	else if (cursor.starts_with("if", false)) {
		return IfParser(this).parse (cursor);
	}
	else if (cursor.starts_with("while", false)) {
		return WhileParser(this).parse (cursor);
	}
	else if (cursor.starts_with("return")) {
		const Type* return_type = get_return_type ();
		Expression* expression = nullptr;
		if (return_type != &Type::VOID) {
			cursor.skip_whitespace ();
			expression = ExpressionParser(this).parse (cursor);
			if (expression->get_type() != return_type)
				cursor.error ("invalid return type");
		}
		set_returned ();
		return new Return (expression);
	}
	else {
		return new ExpressionNode (ExpressionParser(this).parse (cursor));
	}
}

Block* BlockParser::parse (Cursor& cursor) {
	block = new Block ();
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}' && !block->returns && *cursor != '\0') {
		Node* node = LineParser(this).parse (cursor);
		block->append_node (node);
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	return block;
}

If* IfParser::parse (Cursor& cursor) {
	cursor.expect ("if");
	cursor.skip_whitespace ();
	Expression* condition = ExpressionParser(this).parse (cursor);
	if (condition->get_type() != &Type::BOOL) cursor.error ("condition must be of type Bool");
	If* result = new If (condition);
	cursor.skip_whitespace ();
	Block* if_block = BlockParser(this).parse (cursor);
	result->set_if_block (if_block);
	return result;
}

While* WhileParser::parse (Cursor& cursor) {
	cursor.expect ("while");
	cursor.skip_whitespace ();
	Expression* condition = ExpressionParser(this).parse (cursor);
	if (condition->get_type() != &Type::BOOL) cursor.error ("condition must be of type Bool");
	While* result = new While (condition);
	cursor.skip_whitespace ();
	Block* block = BlockParser(this).parse (cursor);
	result->set_block (block);
	return result;
}

Function* FunctionParser::parse (Cursor& cursor) {
	cursor.expect ("func");
	cursor.skip_whitespace ();
	
	// name
	Substring name = IdentifierParser(this).parse (cursor);
	if (get_function(name)) cursor.error ("function already defined");
	function = new Function (name);
	cursor.skip_whitespace ();
	
	// argument list
	BlockParser block_parser (this);
	cursor.expect("(");
	cursor.skip_whitespace ();
	while (*cursor != ')') {
		Substring argument_name = IdentifierParser(this).parse (cursor);
		cursor.skip_whitespace ();
		cursor.expect (":");
		cursor.skip_whitespace ();
		const Type* argument_type = TypeParser(this).parse (cursor, false);
		Variable* argument = block_parser.add_variable (argument_name, argument_type);
		if (!argument) cursor.error ("duplicate argument name");
		function->append_argument (argument);
		cursor.skip_whitespace ();
	}
	cursor.expect (")");
	cursor.skip_whitespace ();
	
	// return type
	if (cursor.starts_with(":")) {
		cursor.skip_whitespace ();
		const Type* return_type = TypeParser(this).parse (cursor, true);
		function->set_return_type (return_type);
		cursor.skip_whitespace ();
	}
	add_function (function);
	
	// code block
	Block* block = block_parser.parse (cursor);
	if (function->get_return_type() != &Type::VOID && !block->returns)
		cursor.error ("missing return statement");
	function->set_block (block);
	return function;
}

Class* ClassParser::parse (Cursor& cursor) {
	cursor.expect ("class");
	cursor.skip_whitespace ();
	Substring name = IdentifierParser(this).parse (cursor);
	_class = new Class (name);
	cursor.skip_whitespace ();
	cursor.expect ("{");
	cursor.skip_whitespace ();
	while (*cursor != '}' && *cursor != '\0') {
		VariableDefinitionParser(this).parse (cursor);
		cursor.skip_whitespace ();
	}
	cursor.expect ("}");
	return _class;
}

static FunctionDeclaration* create_function (const char* name, std::initializer_list<const Type*> arguments, const Type* return_type = &Type::VOID) {
	FunctionDeclaration* function = new FunctionDeclaration (name);
	for (const Type* type: arguments)
		function->append_argument (new Variable (0, type));
	function->set_return_type (return_type);
	return function;
}

Program* ProgramParser::parse (Cursor& cursor) {
	program = new Program ();
	program->add_function_declaration (create_function("print", {&Type::INT}));
	cursor.skip_whitespace ();
	while (*cursor != '\0') {
		if (cursor.starts_with("func", false)) {
			Function* function = FunctionParser(this).parse (cursor);
			//program->add_function (function);
		}
		else {
			Class* _class = ClassParser(this).parse (cursor);
			program->add_class (_class);
		}
		cursor.skip_whitespace ();
	}
	return program;
}
