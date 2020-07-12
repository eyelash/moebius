#pragma once

#include <cstddef>
#include <cstdio>

#include <vector>
#include <fstream>
#include <iterator>

class StringView {
	const char* string;
	std::size_t length;
	static constexpr std::size_t strlen(const char* s, std::size_t i = 0) {
		return *s == '\0' ? i : strlen(s + 1, i + 1);
	}
	static constexpr int strncmp(const char* s0, const char* s1, std::size_t n) {
		return n == 0 ? 0 : *s0 != *s1 ? *s0 - *s1 : strncmp(s0 + 1, s1 + 1, n - 1);
	}
	static constexpr const char* strchr(const char* s, char c) {
		return *s == c ? s : *s == '\0' ? nullptr : strchr(s + 1, c);
	}
public:
	constexpr StringView(): string(nullptr), length(0) {}
	constexpr StringView(const char* string, std::size_t length): string(string), length(length) {}
	constexpr StringView(const char* string): string(string), length(strlen(string)) {}
	constexpr char operator [](std::size_t i) const {
		return string[i];
	}
	constexpr std::size_t get_length() const {
		return length;
	}
	constexpr std::size_t size() const {
		return length;
	}
	constexpr bool operator ==(const StringView& s) const {
		return length != s.length ? false : strncmp(string, s.string, length) == 0;
	}
	constexpr bool operator !=(const StringView& s) const {
		return !operator ==(s);
	}
	constexpr bool operator <(const StringView& s) const {
		return length != s.length ? length < s.length : strncmp(string, s.string, length) < 0;
	}
	constexpr const char* begin() const {
		return string;
	}
	constexpr const char* end() const {
		return string + length;
	}
	constexpr bool contains(char c) const {
		return strchr(string, c);
	}
};

class FilePrinter {
	FILE* file;
public:
	FilePrinter(FILE* file = stdout): file(file) {}
	void print(char c) {
		fputc(c, file);
	}
	void print(const StringView& s) {
		fwrite(s.begin(), 1, s.size(), file);
	}
	void print(const char* s) {
		print(StringView(s));
	}
	template <class T> void print(const T& t) {
		t.print(*this);
	}
	template <class T> void println(const T& t) {
		print(t);
		print('\n');
	}
};

class MemoryPrinter {
	std::vector<char> data;
public:
	void print(char c) {
		data.push_back(c);
	}
	void print(const StringView& s) {
		data.insert(data.end(), s.begin(), s.end());
	}
	void print(const char* s) {
		print(StringView(s));
	}
	template <class T> void print(const T& t) {
		t.print(*this);
	}
	template <class T> void println(const T& t) {
		print(t);
		print('\n');
	}
	void print_to_file(FILE* file) {
		fwrite(data.data(), 1, data.size(), file);
	}
};

template <class... T> class PrintTuple;
template <> class PrintTuple<> {
public:
	constexpr PrintTuple() {}
	template <class P> void print(P& p) const {}
	template <class P> void print_formatted(P& p, const char* s) const {
		p.print(s);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	const T0& t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(const T0& t0, const T&... t): t0(t0), t(t...) {}
	template <class P> void print(P& p) const {
		p.print(t0);
		p.print(t);
	}
	template <class P> void print_formatted(P& p, const char* s) const {
		while (*s) {
			if (*s == '%') {
				++s;
				if (*s != '%') {
					p.print(t0);
					t.print_formatted(p, s);
					return;
				}
			}
			p.print(*s);
			++s;
		}
	}
};
template <class... T> constexpr PrintTuple<T...> print_tuple(const T&... t) {
	return PrintTuple<T...>(t...);
}

template <class... T> class Format {
	PrintTuple<T...> t;
	const char* s;
public:
	constexpr Format(const char* s, const T&... t): t(t...), s(s) {}
	template <class P> void print(P& p) const {
		t.print_formatted(p, s);
	}
};
template <class... T> constexpr Format<T...> format(const char* s, const T&... t) {
	return Format<T...>(s, t...);
}

template <class T> auto bold(const T& t) {
	return print_tuple("\e[1m", t, "\e[m");
}
template <class T> auto red(const T& t) {
	return print_tuple("\e[31m", t, "\e[m");
}
template <class T> auto green(const T& t) {
	return print_tuple("\e[32m", t, "\e[m");
}
template <class T> auto yellow(const T& t) {
	return print_tuple("\e[33m", t, "\e[m");
}

class PrintNumber {
	unsigned int n;
public:
	constexpr PrintNumber(unsigned int n): n(n) {}
	template <class P> void print(P& p) const {
		if (n >= 10) {
			p.print(PrintNumber(n / 10));
		}
		p.print(static_cast<char>('0' + n % 10));
	}
};
inline PrintNumber print_number(unsigned int n) {
	return PrintNumber(n);
}

template <class T> class Indent {
	const T& t;
	unsigned int indentation;
public:
	constexpr Indent(const T& t, unsigned int indentation): t(t), indentation(indentation) {}
	template <class P> void print(P& p) const {
		for (std::size_t i = 0; i < indentation * 2; ++i) {
			p.print(' ');
		}
		p.print(t);
	}
};

class SourceFile {
	const char* file_name;
	std::vector<char> content;
public:
	SourceFile(const char* file_name): file_name(file_name) {
		std::ifstream file(file_name);
		content.insert(content.end(), std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
	const char* get_name() const {
		return file_name;
	}
	const char* begin() const {
		return content.data();
	}
	const char* end() const {
		return content.data() + content.size();
	}
};

class SourcePosition {
	const char* file_name;
	std::size_t position;
public:
	SourcePosition(const char* file_name, std::size_t position): file_name(file_name), position(position) {}
	SourcePosition(): file_name(nullptr) {}
	template <class T> void print_error(FilePrinter& printer, const T& t) const {
		SourceFile file(file_name);
		unsigned int line_number = 1;
		const char* c = file.begin();
		const char* end = file.end();
		const char* position = std::min(c + this->position, end);
		const char* line_start = c;
		while (c < position) {
			if (*c == '\n') {
				++c;
				++line_number;
				line_start = c;
			}
			else {
				++c;
			}
		}
		unsigned int column = 1 + (c - line_start);

		printer.print(bold(format("%:%:%: ", file_name, print_number(line_number), print_number(column))));
		printer.print(bold(red("error: ")));
		printer.print(t);
		printer.print('\n');

		c = line_start;
		while (c < end && *c != '\n') {
			printer.print(*c);
			++c;
		}
		printer.print('\n');

		c = line_start;
		while (c < position) {
			printer.print(*c == '\t' ? '\t' : ' ');
			++c;
		}
		printer.print(bold(red('^')));
		printer.print('\n');
	}
};
