#pragma once

#include "printer.hpp"
#include <map>
#include <cstdlib>

class Expression;

struct BinaryOperator {
	const char* string;
	using Create = Expression* (*)(const Expression* left, const Expression* right);
	Create create;
	constexpr BinaryOperator(const char* string, Create create): string(string), create(create) {}
};

using OperatorLevel = std::initializer_list<BinaryOperator>;

constexpr std::initializer_list<OperatorLevel> operators = {};

struct UnaryOperator {
	const char* string;
	using Create = Expression* (*)(const Expression* expression);
	Create create;
	constexpr UnaryOperator(const char* string, Create create): string(string), create(create) {}
};

constexpr std::initializer_list<UnaryOperator> unary_operators = {};

class Cursor {
	const SourceFile* file;
	const char* position;
public:
	Cursor(const SourceFile* file): file(file), position(file->begin()) {}
	constexpr Cursor(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr bool operator <(const Cursor& rhs) const {
		return position < rhs.position;
	}
	constexpr char operator *() const {
		return *position;
	}
	Cursor& operator ++() {
		++position;
		return *this;
	}
	constexpr StringView operator -(const Cursor& start) const {
		return StringView(start.position, position - start.position);
	}
	const char* get_path() const {
		return file->get_path();
	}
	std::size_t get_position() const {
		return position - file->begin();
	}
};

template <class F> class CharParser {
	F f;
public:
	constexpr CharParser(F f): f(f) {}
	template <class C> bool parse(C& cursor) {
		if (cursor && f(*cursor)) {
			++cursor;
			return true;
		}
		return false;
	}
};

class StringParser {
	StringView s;
public:
	constexpr StringParser(const StringView& s): s(s) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		for (char c: s) {
			if (!(copy && *copy == c)) {
				return false;
			}
			++copy;
		}
		cursor = copy;
		return true;
	}
};

template <class P0, class P1> class SequenceParser {
	P0 p0;
	P1 p1;
public:
	constexpr SequenceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		if (!p0.parse(copy)) {
			return false;
		}
		if (!p1.parse(copy)) {
			return false;
		}
		cursor = copy;
		return true;
	}
};

template <class P0, class P1> class ChoiceParser {
	P0 p0;
	P1 p1;
public:
	constexpr ChoiceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
	template <class C> bool parse(C& cursor) {
		if (p0.parse(cursor)) {
			return true;
		}
		if (p1.parse(cursor)) {
			return true;
		}
		return false;
	}
};

template <class P> class RepeatParser {
	P p;
public:
	constexpr RepeatParser(P p): p(p) {}
	template <class C> bool parse(C& cursor) {
		while (p.parse(cursor)) {}
		return true;
	}
};

template <class P> class NotParser {
	P p;
public:
	constexpr NotParser(P p): p(p) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		if (p.parse(copy)) {
			return false;
		}
		return true;
	}
};

template <class P> class PeekParser {
	P p;
public:
	constexpr PeekParser(P p): p(p) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		if (p.parse(copy)) {
			return true;
		}
		return false;
	}
};

template <class P, class = bool> struct is_parser: std::false_type {};
template <class P> struct is_parser<P, decltype(std::declval<P>().parse(std::declval<Cursor&>()))>: std::true_type {};

template <class F, class = bool> struct is_char_class: std::false_type {};
template <class F> struct is_char_class<F, decltype(std::declval<F>()(std::declval<char>()))>: std::true_type {};

class Parser {
	Cursor cursor;
public:
	static constexpr auto get_parser(char c) {
		return CharParser([c](char c2) {
			return c == c2;
		});
	}
	static constexpr StringParser get_parser(const StringView& s) {
		return StringParser(s);
	}
	static constexpr StringParser get_parser(const char* s) {
		return StringParser(s);
	}
	template <class P> static constexpr std::enable_if_t<is_parser<P>::value, P> get_parser(P p) {
		return p;
	}
	template <class F> static constexpr std::enable_if_t<is_char_class<F>::value, CharParser<F>> get_parser(F f) {
		return CharParser(f);
	}
	static constexpr auto range(char first, char last) {
		return CharParser([first, last](char c) {
			return c >= first && c <= last;
		});
	}
	template <class P0, class P1> static constexpr auto sequence(P0 p0, P1 p1) {
		return SequenceParser(get_parser(p0), get_parser(p1));
	}
	template <class P0, class P1, class P2, class... P> static constexpr auto sequence(P0 p0, P1 p1, P2 p2, P... p) {
		return sequence(sequence(p0, p1), p2, p...);
	}
	template <class P0, class P1> static constexpr auto choice(P0 p0, P1 p1) {
		return ChoiceParser(get_parser(p0), get_parser(p1));
	}
	template <class P0, class P1, class P2, class... P> static constexpr auto choice(P0 p0, P1 p1, P2 p2, P... p) {
		return choice(choice(p0, p1), p2, p...);
	}
	template <class P> static constexpr auto zero_or_more(P p) {
		return RepeatParser(get_parser(p));
	}
	template <class P> static constexpr auto one_or_more(P p) {
		return sequence(p, zero_or_more(p));
	}
	template <class P> static constexpr auto not_(P p) {
		return NotParser(get_parser(p));
	}
	template <class P> static constexpr auto peek(P p) {
		return PeekParser(get_parser(p));
	}
	static constexpr bool any_char(char c) {
		return true;
	}
	static constexpr bool white_space(char c) {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r';
	}
	static constexpr bool numeric(char c) {
		return c >= '0' && c <= '9';
	}
	static constexpr bool alphabetic(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}
	static constexpr bool alphanumeric(char c) {
		return alphabetic(c) || numeric(c);
	}
	static constexpr bool operator_char(char c) {
		return StringView("+-*/%=<>!&|~^?:").contains(c);
	}
	template <class P> StringView parse(P p) {
		const Cursor start = cursor;
		if (get_parser(p).parse(cursor)) {
			return cursor - start;
		}
		else {
			return StringView();
		}
	}
	Parser(const SourceFile* file): cursor(file) {}
	Parser(const Cursor& cursor): cursor(cursor) {}
	const char* get_path() const {
		return cursor.get_path();
	}
	std::size_t get_position() const {
		return cursor.get_position();
	}
};
