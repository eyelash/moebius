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
	constexpr StringView substr(std::size_t pos, std::size_t length) const {
		return StringView(string + pos, length);
	}
	constexpr StringView substr(std::size_t pos) const {
		return substr(pos, length - pos);
	}
	constexpr bool starts_with(const StringView& s) const {
		return length < s.length ? false : strncmp(string, s.string, s.length) == 0;
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
	template <class T0, class... T> void print(const char* s, const T0& t0, const T&... t) {
		while (*s) {
			if (*s == '%') {
				++s;
				if (*s != '%') {
					print(t0);
					print(s, t...);
					return;
				}
			}
			print(*s);
			++s;
		}
	}
};
