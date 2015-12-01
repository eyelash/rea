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
#include <cctype>

#define CSI "\e["
#define RESET CSI "m"
#define BOLD CSI "1" "m"
#define RED CSI "31" "m"
#define YELLOW CSI "33" "m"

class Cursor {
	const char* string;
	int position;
	int line;
	Function* function;
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
		fputs ("^\n", stderr);
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
		while (isspace(string[position]))
			advance ();
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
	char operator [] (int i) const {
		return string[position + i];
	}
	Cursor& operator ++ () {
		advance ();
		return *this;
	}
	char operator * () const {
		return string[position];
	}
	void set_function (Function* function) {
		this->function = function;
	}
	Function* get_function () {
		return function;
	}
};
