#pragma once

#include <cstddef>
#include <iostream>
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
	constexpr operator bool() const {
		return string != nullptr;
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
	constexpr StringView substr(std::size_t pos, std::size_t count) const {
		return StringView(string + pos, count);
	}
	constexpr StringView substr(std::size_t pos) const {
		return StringView(string + pos, length - pos);
	}
};

inline std::int32_t next_codepoint(StringView& s) {
	std::int32_t codepoint = 0;
	if (s.size() >= 1 && (s[0] & 0b1000'0000) == 0b0000'0000) {
		codepoint = s[0];
		s = s.substr(1);
	}
	else if (s.size() >= 2 && (s[0] & 0b1110'0000) == 0b1100'0000) {
		codepoint |= (s[0] & 0b0001'1111) << 6;
		codepoint |= (s[1] & 0b0011'1111);
		s = s.substr(2);
	}
	else if (s.size() >= 3 && (s[0] & 0b1111'0000) == 0b1110'0000) {
		codepoint |= (s[0] & 0b0000'1111) << 12;
		codepoint |= (s[1] & 0b0011'1111) << 6;
		codepoint |= (s[2] & 0b0011'1111);
		s = s.substr(3);
	}
	else if (s.size() >= 4 && (s[0] & 0b1111'1000) == 0b1111'0000) {
		codepoint |= (s[0] & 0b0000'0111) << 18;
		codepoint |= (s[1] & 0b0011'1111) << 12;
		codepoint |= (s[2] & 0b0011'1111) << 6;
		codepoint |= (s[3] & 0b0011'1111);
		s = s.substr(4);
	}
	return codepoint;
}

inline std::string from_codepoint(std::int32_t codepoint) {
	std::string s;
	if (codepoint < 0b1000'0000) {
		s.push_back(codepoint);
	}
	else if (codepoint < 0b1000'0000'0000) {
		s.push_back(0b1100'0000 | codepoint >> 6);
		s.push_back(0b1000'0000 | codepoint & 0b0011'1111);
	}
	else if (codepoint < 0b0001'0000'0000'0000'0000) {
		s.push_back(0b1110'0000 | codepoint >> 12);
		s.push_back(0b1000'0000 | codepoint >> 6 & 0b0011'1111);
		s.push_back(0b1000'0000 | codepoint & 0b0011'1111);
	}
	else if (codepoint < 0b0010'0000'0000'0000'0000'0000) {
		s.push_back(0b1111'0000 | codepoint >> 18);
		s.push_back(0b1000'0000 | codepoint >> 12 & 0b0011'1111);
		s.push_back(0b1000'0000 | codepoint >> 6 & 0b0011'1111);
		s.push_back(0b1000'0000 | codepoint & 0b0011'1111);
	}
	return s;
}

class CodePoints {
	StringView s;
public:
	class Iterator {
		StringView s;
		std::int32_t codepoint;
	public:
		Iterator(const StringView& s): s(s), codepoint(next_codepoint(this->s)) {}
		Iterator(): s(), codepoint(0) {}
		bool operator !=(const Iterator& rhs) const {
			return codepoint != rhs.codepoint;
		}
		std::int32_t operator *() const {
			return codepoint;
		}
		Iterator& operator ++() {
			codepoint = next_codepoint(s);
			return *this;
		}
	};
	constexpr CodePoints(const StringView& s): s(s) {}
	Iterator begin() const {
		return Iterator(s);
	}
	Iterator end() const {
		return Iterator();
	}
};
inline CodePoints code_points(const StringView& s) {
	return CodePoints(s);
}
inline CodePoints code_points(const std::string& s) {
	return CodePoints(StringView(s.data(), s.size()));
}

class Printer {
	std::ostream& ostream;
public:
	Printer(std::ostream& ostream = std::cout): ostream(ostream) {}
	void print(char c) const {
		ostream.put(c);
	}
	void print(const StringView& s) const {
		ostream.write(s.begin(), s.size());
	}
	void print(const char* s) const {
		print(StringView(s));
	}
	void print(const std::string& s) const {
		ostream.write(s.data(), s.size());
	}
	template <class T> void print(const T& t) const {
		t.print(*this);
	}
	template <class T> void println(const T& t) const {
		print(t);
		print('\n');
	}
};

template <class F> class PrintFunctor {
	F f;
public:
	constexpr PrintFunctor(F f): f(f) {}
	void print(const Printer& p) const {
		f(p);
	}
};
template <class F> constexpr PrintFunctor<F> print_functor(F f) {
	return PrintFunctor<F>(f);
}

template <class... T> class PrintTuple;
template <> class PrintTuple<> {
public:
	constexpr PrintTuple() {}
	void print(const Printer& p) const {}
	void print_formatted(const Printer& p, const char* s) const {
		p.print(s);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	const T0& t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(const T0& t0, const T&... t): t0(t0), t(t...) {}
	void print(const Printer& p) const {
		p.print(t0);
		p.print(t);
	}
	void print_formatted(const Printer& p, const char* s) const {
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
	void print(const Printer& p) const {
		t.print_formatted(p, s);
	}
};
template <class... T> constexpr Format<T...> format(const char* s, const T&... t) {
	return Format<T...>(s, t...);
}

constexpr auto bold = [](const auto& t) {
	return print_tuple("\x1B[1m", t, "\x1B[22m");
};
constexpr auto red = [](const auto& t) {
	return print_tuple("\x1B[31m", t, "\x1B[39m");
};
constexpr auto green = [](const auto& t) {
	return print_tuple("\x1B[32m", t, "\x1B[39m");
};
constexpr auto yellow = [](const auto& t) {
	return print_tuple("\x1B[33m", t, "\x1B[39m");
};

class PrintNumber {
	unsigned int n;
public:
	constexpr PrintNumber(unsigned int n): n(n) {}
	void print(const Printer& p) const {
		if (n >= 10) {
			p.print(PrintNumber(n / 10));
		}
		p.print(static_cast<char>('0' + n % 10));
	}
};
constexpr PrintNumber print_number(unsigned int n) {
	return PrintNumber(n);
}

class PrintHexadecimal {
	unsigned int n;
	unsigned int digits;
	static constexpr char get_hex(unsigned int c) {
		return c < 10 ? '0' + c : 'A' + (c - 10);
	}
public:
	constexpr PrintHexadecimal(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(const Printer& p) const {
		if (n >= 16 || digits > 1) {
			p.print(PrintHexadecimal(n / 16, digits > 1 ? digits - 1 : digits));
		}
		p.print(get_hex(n % 16));
	}
};
constexpr PrintHexadecimal print_hexadecimal(unsigned int n, unsigned int digits = 1) {
	return PrintHexadecimal(n, digits);
}
template <class T> constexpr PrintHexadecimal print_pointer(const T* ptr) {
	return PrintHexadecimal(reinterpret_cast<std::size_t>(ptr));
}

class PrintOctal {
	unsigned int n;
	unsigned int digits;
public:
	constexpr PrintOctal(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(const Printer& p) const {
		if (n >= 8 || digits > 1) {
			p.print(PrintOctal(n / 8, digits > 1 ? digits - 1 : digits));
		}
		p.print(static_cast<char>('0' + n % 8));
	}
};
constexpr PrintOctal print_octal(unsigned int n, unsigned int digits = 1) {
	return PrintOctal(n, digits);
}

class PrintPlural {
	const char* word;
	unsigned int count;
public:
	constexpr PrintPlural(const char* word, unsigned int count): word(word), count(count) {}
	void print(const Printer& p) const {
		p.print(print_number(count));
		p.print(' ');
		p.print(word);
		if (count != 1) {
			p.print('s');
		}
	}
};

constexpr PrintPlural print_plural(const char* word, unsigned int count) {
	return PrintPlural(word, count);
}

class IndentPrinter {
	Printer printer;
	unsigned int indentation = 0;
public:
	IndentPrinter(std::ostream& ostream): printer(ostream) {}
	template <class T> void println(const T& t) const {
		for (unsigned int i = 0; i < indentation; ++i) {
			printer.print('\t');
		}
		printer.println(t);
	}
	template <class T> void println_increasing(const T& t) {
		println(t);
		indentation += 1;
	}
	template <class T> void println_decreasing(const T& t) {
		indentation -= 1;
		println(t);
	}
};

class SourceFile {
	const char* path;
	std::vector<char> content;
public:
	SourceFile(const char* path): path(path) {
		std::ifstream file(path);
		content.insert(content.end(), std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}
	const char* get_path() const {
		return path;
	}
	const char* begin() const {
		return content.data();
	}
	const char* end() const {
		return content.data() + content.size();
	}
};

template <class T, class C> void print_message(const Printer& printer, const C& color, const char* severity, const T& t) {
	printer.print(bold(color(format("%: ", severity))));
	printer.print(t);
	printer.print('\n');
}
template <class T, class C> void print_message(const Printer& printer, const char* path, std::size_t source_position, const C& color, const char* severity, const T& t) {
	if (path == nullptr) {
		print_message(printer, color, severity, t);
	}
	else {
		SourceFile file(path);
		unsigned int line_number = 1;
		const char* c = file.begin();
		const char* end = file.end();
		const char* position = std::min(c + source_position, end);
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
		const unsigned int column = 1 + (c - line_start);

		printer.print(bold(format("%:%:%: ", path, print_number(line_number), print_number(column))));
		print_message(printer, color, severity, t);

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
		printer.print(bold(color('^')));
		printer.print('\n');
	}
}
template <class T> void print_error(const Printer& printer, const T& t) {
	print_message(printer, red, "error", t);
}
template <class T> void print_error(const Printer& printer, const char* path, std::size_t source_position, const T& t) {
	print_message(printer, path, source_position, red, "error", t);
}

class Variable {
	std::size_t index;
public:
	constexpr Variable(std::size_t index): index(index) {}
	Variable() {}
	void print(const Printer& printer) const {
		printer.print(format("v%", print_number(index)));
	}
};
