#pragma once

#include <cstddef>
#include <cstdio>

class StringView {
	const char* string;
	std::size_t length;
	static constexpr std::size_t strlen(const char* s, std::size_t i = 0) {
		return *s == '\0' ? i : strlen(s + 1, i + 1);
	}
	static constexpr int strncmp(const char* s0, const char* s1, std::size_t n) {
		return n == 0 ? 0 : *s0 != *s1 ? *s0 - *s1 : strncmp(s0 + 1, s1 + 1, n - 1);
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
};

class Printer {
	FILE* file;
public:
	Printer(FILE* file = stdout): file(file) {}
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

template <class... T> class PrintTuple;
template <> class PrintTuple<> {
public:
	constexpr PrintTuple() {}
	void print(Printer& p) const {}
	void print_formatted(Printer& p, const char* s) const {
		p.print(s);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	const T0& t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(const T0& t0, const T&... t): t0(t0), t(t...) {}
	void print(Printer& p) const {
		p.print(t0);
		p.print(t);
	}
	void print_formatted(Printer& p, const char* s) const {
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
	void print(Printer& p) const {
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
	void print(Printer& p) const {
		if (n >= 10) {
			p.print(PrintNumber(n / 10));
		}
		p.print(static_cast<char>('0' + n % 10));
	}
};
inline PrintNumber print_number(unsigned int n) {
	return PrintNumber(n);
}
