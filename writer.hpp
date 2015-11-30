/*

Copyright (c) 2015, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "ast.hpp"
#include <cstdio>

#define INDENT "  "

class Writer {
	FILE* file;
	int value_counter;
	int label_counter;
public:
	Writer (): file(stdout), value_counter(0), label_counter(0) {}
	Writer& operator << (const char* str) {
		fputs (str, file);
		return *this;
	}
	Writer& operator << (int n) {
		fprintf (file, "%d", n);
		return *this;
	}
	Writer& operator << (const Substring& substring) {
		substring.write (file);
		return *this;
	}
	Writer& operator << (Node* node) {
		node->write (*this);
		return *this;
	}
	Writer& operator << (Expression* expression) {
		expression->insert (*this);
		return *this;
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
