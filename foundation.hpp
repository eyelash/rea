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

#include <cstdio>
#include <cstdlib>
#include <cstring>

class Character {
	char c;
public:
	Character (char c): c(c) {}
	operator char () const {
		return c;
	}
	bool is_whitespace () const {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',';
	}
	bool is_alphabetic () const {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}
	bool is_numeric () const {
		return c >= '0' && c <= '9';
	}
	bool is_alphanumeric () const {
		return is_alphabetic() || is_numeric();
	}
};

class String {
	char* data;
public:
	String (const char* file_name) {
		FILE* file = fopen (file_name, "r");
		if (!file) {
			data = nullptr;
			fprintf (stderr, "error: cannot open file\n");
			return;
		}
		fseek (file, 0, SEEK_END);
		int length = ftell (file);
		rewind (file);
		data = (char*) malloc (length+1);
		fread (data, 1, length, file);
		data[length] = '\0';
		fclose (file);
	}
	~String () {
		free (data);
	}
	Character operator [] (int i) const {
		return data[i];
	}
	const char* get_data () const {
		return data;
	}
};

class Substring {
	const char* start;
	int length;
public:
	Substring (const char* start, int length): start(start), length(length) {}
	Substring (const char* string): start(string), length(strlen(string)) {}
	void write (FILE* file) const {
		fwrite (start, 1, length, file);
	}
	bool operator == (const Substring& s) const {
		if (length != s.length) return false;
		return strncmp (start, s.start, length) == 0;
	}
	bool operator < (const Substring& s) const {
		int r = strncmp (start, s.start, length < s.length ? length : s.length);
		if (r == 0) return length < s.length;
		else return r < 0;
	}
};
