#include "ast.hpp"
#include <cstddef>

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

struct BinaryOperator {
	const char* string;
	using Create = Expression* (*)(Expression* left, Expression* right);
	Create create;
	constexpr BinaryOperator(const char* string, Create create): string(string), create(create) {}
	constexpr BinaryOperator(): string(nullptr), create(nullptr) {}
	constexpr operator bool() const {
		return string != nullptr;
	}
};

static constexpr BinaryOperator operators[][4] = {
	{
		BinaryOperator("+", Addition::create),
		BinaryOperator("-", Subtraction::create)
	},
	{
		BinaryOperator("*", Multiplication::create),
		BinaryOperator("/", Division::create),
		BinaryOperator("%", Remainder::create)
	}
};

class Parser {
	StringView string;
	static constexpr bool white_space(char c) {
		return c == ' ' || c == '\n' || c == '\t';
	}
	static constexpr bool numeric(char c) {
		return c >= '0' && c <= '9';
	}
	template <class F> void parse_all(F f) {
		std::size_t i = 0;
		while (i < string.size() && f(string[i])) {
			++i;
		}
		string = string.substr(i);
	}
	bool parse(char c) {
		if (0 < string.size() && string[0] == c) {
			string = string.substr(1);
			return true;
		}
		return false;
	}
	bool parse(const StringView& s) {
		if (string.starts_with(s)) {
			string = string.substr(s.size());
			return true;
		}
		return false;
	}
	Expression* parse_number() {
		std::int32_t number = 0;
		while (0 < string.size() && numeric(string[0])) {
			number *= 10;
			number += string[0] - '0';
			string = string.substr(1);
		}
		return new Number(number);
	}
	BinaryOperator parse_operator(int level) {
		for (int i = 0; operators[level][i]; ++i) {
			if (parse(operators[level][i].string)) {
				return operators[level][i];
			}
		}
		return BinaryOperator();
	}
	Expression* parse_expression_last() {
		if (parse("(")) {
			parse_all(white_space);
			Expression* expression = parse_expression();
			parse_all(white_space);
			parse(")");
			return expression;
		}
		return parse_number();
	}
	Expression* parse_expression(int level = 0) {
		if (level == 2) {
			return parse_expression_last();
		}
		Expression* left = parse_expression(level + 1);
		parse_all(white_space);
		while (BinaryOperator op = parse_operator(level)) {
			parse_all(white_space);
			Expression* right = parse_expression(level + 1);
			left = op.create(left, right);
			parse_all(white_space);
		}
		return left;
	}
public:
	Parser(const char* string): string(string) {}
	Expression* parse() {
		parse_all(white_space);
		return parse_expression();
	}
};
