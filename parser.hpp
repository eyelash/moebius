#pragma once

#include "common.hpp"
#include "printer.hpp"
#include "ast.hpp"
#include <map>
#include <cstdlib>

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

template <class F> struct CharParser {
	F f;
	constexpr CharParser(F f): f(f) {}
};

struct StringParser {
	StringView s;
	constexpr StringParser(const StringView& s): s(s) {}
};

template <class P0, class P1> struct SequenceParser {
	P0 p0;
	P1 p1;
	constexpr SequenceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
};

template <class P0, class P1> struct ChoiceParser {
	P0 p0;
	P1 p1;
	constexpr ChoiceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
};

template <class P> struct RepeatParser {
	P p;
	constexpr RepeatParser(P p): p(p) {}
};

template <class P> struct NotParser {
	P p;
	constexpr NotParser(P p): p(p) {}
};

template <class P> struct PeekParser {
	P p;
	constexpr PeekParser(P p): p(p) {}
};

template <class P> struct is_parser: std::false_type {};
template <class F> struct is_parser<CharParser<F>>: std::true_type {};
template <> struct is_parser<StringParser>: std::true_type {};
template <class P0, class P1> struct is_parser<SequenceParser<P0, P1>>: std::true_type {};
template <class P0, class P1> struct is_parser<ChoiceParser<P0, P1>>: std::true_type {};
template <class P> struct is_parser<RepeatParser<P>>: std::true_type {};
template <class P> struct is_parser<NotParser<P>>: std::true_type {};
template <class P> struct is_parser<PeekParser<P>>: std::true_type {};

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
	template <class F> static bool parse(const CharParser<F>& p, Cursor& cursor) {
		if (cursor && p.f(*cursor)) {
			++cursor;
			return true;
		}
		return false;
	}
	static bool parse(const StringParser& p, Cursor& cursor) {
		Cursor copy = cursor;
		for (char c: p.s) {
			if (!(copy && *copy == c)) {
				return false;
			}
			++copy;
		}
		cursor = copy;
		return true;
	}
	template <class P0, class P1> static bool parse(const SequenceParser<P0, P1>& p, Cursor& cursor) {
		Cursor copy = cursor;
		if (!parse(p.p0, copy)) {
			return false;
		}
		if (!parse(p.p1, copy)) {
			return false;
		}
		cursor = copy;
		return true;
	}
	template <class P0, class P1> static bool parse(const ChoiceParser<P0, P1>& p, Cursor& cursor) {
		if (parse(p.p0, cursor)) {
			return true;
		}
		if (parse(p.p1, cursor)) {
			return true;
		}
		return false;
	}
	template <class P> static bool parse(const RepeatParser<P>& p, Cursor& cursor) {
		while (parse(p.p, cursor)) {}
		return true;
	}
	template <class P> static bool parse(const NotParser<P>& p, Cursor& cursor) {
		Cursor copy = cursor;
		if (parse(p.p, copy)) {
			return false;
		}
		return true;
	}
	template <class P> static bool parse(const PeekParser<P>& p, Cursor& cursor) {
		Cursor copy = cursor;
		if (parse(p.p, copy)) {
			return true;
		}
		return false;
	}
	template <class P> StringView parse(P p) {
		const Cursor start = cursor;
		if (parse(get_parser(p), cursor)) {
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

using BinaryCreate = Expression* (*)(Expression* left, Expression* right);

template <class P> struct BinaryOperator {
	P p;
	BinaryCreate create;
	constexpr BinaryOperator(P p, BinaryCreate create): p(p), create(create) {}
};

using UnaryCreate = Expression* (*)(Expression* expression);

template <class P> struct UnaryOperator {
	P p;
	UnaryCreate create;
	constexpr UnaryOperator(P p, UnaryCreate create): p(p), create(create) {}
};

template <class... T> struct BinaryLeftToRight {
	Tuple<T...> tuple;
	constexpr BinaryLeftToRight(T... tuple): tuple(tuple...) {}
};

template <class... T> struct BinaryRightToLeft {
	Tuple<T...> tuple;
	constexpr BinaryRightToLeft(T... tuple): tuple(tuple...) {}
};

template <class... T> struct UnaryPrefix {
	Tuple<T...> tuple;
	constexpr UnaryPrefix(T... tuple): tuple(tuple...) {}
};

template <class... T> struct UnaryPostfix {
	Tuple<T...> tuple;
	constexpr UnaryPostfix(T... tuple): tuple(tuple...) {}
};

template <class... T> struct OperatorLevels {
	Tuple<T...> tuple;
	constexpr OperatorLevels(T... tuple): tuple(tuple...) {}
};

class MoebiusParser: private Parser {
	static constexpr auto keyword(const StringView& s) {
		return sequence(s, not_(alphanumeric));
	}
	static constexpr auto operator_(const StringView& s) {
		return sequence(s, not_(operator_char));
	}
	template <class P> static constexpr auto binary_operator(P p, BinaryCreate create) {
		return BinaryOperator(get_parser(p), create);
	}
	template <class P> static constexpr auto unary_operator(P p, UnaryCreate create) {
		return UnaryOperator(get_parser(p), create);
	}
	template <class... T> static constexpr auto binary_left_to_right(T... t) {
		return BinaryLeftToRight(t...);
	}
	template <class... T> static constexpr auto binary_right_to_left(T... t) {
		return BinaryRightToLeft(t...);
	}
	template <class... T> static constexpr auto unary_prefix(T... t) {
		return UnaryPrefix(t...);
	}
	template <class... T> static constexpr auto unary_postfix(T... t) {
		return UnaryPostfix(t...);
	}
	template <class... T> static constexpr auto operator_levels(T... t) {
		return OperatorLevels(t...);
	}
	template <class T> [[noreturn]] void error(std::size_t position, const T& t) {
		print_error(Printer(std::cerr), get_path(), position, t);
		std::exit(EXIT_FAILURE);
	}
	template <class T> [[noreturn]] void error(const T& t) {
		error(get_position(), t);
	}
	void expect(const StringView& s) {
		if (!parse(s)) {
			error(format("expected \"%\"", s));
		}
	}
	void expect_keyword(const StringView& s) {
		if (!parse(keyword(s))) {
			error(format("expected \"%\"", s));
		}
	}
	bool parse_comment() {
		if (parse("//")) {
			parse(zero_or_more(sequence(not_("\n"), any_char)));
			return true;
		}
		if (parse("/*")) {
			parse(zero_or_more(sequence(not_("*/"), any_char)));
			expect("*/");
			return true;
		}
		return false;
	}
	void parse_white_space() {
		parse(zero_or_more(white_space));
		while (parse_comment()) {
			parse(zero_or_more(white_space));
		}
	}
	std::nullptr_t parse_operator(const Tuple<>& tuple) {
		return nullptr;
	}
	template <class T0, class... T> auto parse_operator(const Tuple<T0, T...>& tuple) -> decltype(tuple.head.create) {
		if (parse(tuple.head.p)) {
			return tuple.head.create;
		}
		return parse_operator(tuple.tail);
	}
	Expression* parse_expression_last() {
		if (parse('(')) {
			parse_white_space();
			Expression* expression = parse_expression();
			parse_white_space();
			expect(")");
			return expression;
		}
		else if (parse(keyword("if"))) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Expression* condition = parse_expression();
			parse_white_space();
			expect(")");
			parse_white_space();
			Expression* then_expression = parse_expression();
			parse_white_space();
			expect_keyword("else");
			parse_white_space();
			Expression* else_expression = parse_expression();
			return new If(condition, then_expression, else_expression);
		}
		else if (parse(keyword("false"))) {
			return new IntLiteral(0);
		}
		else if (parse(keyword("true"))) {
			return new IntLiteral(1);
		}
		else if (parse(peek(numeric))) {
			std::int32_t number = 0;
			for (char c: parse(zero_or_more(numeric))) {
				number *= 10;
				number += c - '0';
			}
			return new IntLiteral(number);
		}
		else {
			error("expected an expression");
		}
	}
	Expression* parse_expression(const Tuple<>& tuple) {
		return parse_expression_last();
	}
	template <class... T0, class... T> Expression* parse_expression(const Tuple<BinaryLeftToRight<T0...>, T...>& tuple) {
		Expression* left = parse_expression(tuple.tail);
		parse_white_space();
		while (auto create = parse_operator(tuple.head.tuple)) {
			parse_white_space();
			Expression* right = parse_expression(tuple.tail);
			left = create(left, right);
			parse_white_space();
		}
		return left;
	}
	template <class... T0, class... T> Expression* parse_expression(const Tuple<BinaryRightToLeft<T0...>, T...>& tuple) {
		Expression* left = parse_expression(tuple.tail);
		parse_white_space();
		if (auto create = parse_operator(tuple.head.tuple)) {
			parse_white_space();
			Expression* right = parse_expression(tuple);
			left = create(left, right);
		}
		return left;
	}
	template <class... T0, class... T> Expression* parse_expression(const Tuple<UnaryPrefix<T0...>, T...>& tuple) {
		if (auto create = parse_operator(tuple.head.tuple)) {
			parse_white_space();
			Expression* expression = parse_expression(tuple);
			return create(expression);
		}
		else {
			return parse_expression(tuple.tail);
		}
	}
	template <class... T0, class... T> Expression* parse_expression(const Tuple<UnaryPostfix<T0...>, T...>& tuple) {
		Expression* expression = parse_expression(tuple.tail);
		parse_white_space();
		while (auto create = parse_operator(tuple.head.tuple)) {
			expression = create(expression);
			parse_white_space();
		}
		return expression;
	}
	Expression* parse_expression() {
		constexpr auto operators = operator_levels(
			binary_left_to_right(
				binary_operator(operator_("=="), BinaryExpression::create<BinaryOperation::EQ>),
				binary_operator(operator_("!="), BinaryExpression::create<BinaryOperation::NE>)
			),
			binary_left_to_right(
				binary_operator(operator_("<"), BinaryExpression::create<BinaryOperation::LT>),
				binary_operator(operator_("<="), BinaryExpression::create<BinaryOperation::LE>),
				binary_operator(operator_(">"), BinaryExpression::create<BinaryOperation::GT>),
				binary_operator(operator_(">="), BinaryExpression::create<BinaryOperation::GE>)
			),
			binary_left_to_right(
				binary_operator(operator_("+"), BinaryExpression::create<BinaryOperation::ADD>),
				binary_operator(operator_("-"), BinaryExpression::create<BinaryOperation::SUB>)
			),
			binary_left_to_right(
				binary_operator(operator_("*"), BinaryExpression::create<BinaryOperation::MUL>),
				binary_operator(operator_("/"), BinaryExpression::create<BinaryOperation::DIV>),
				binary_operator(operator_("%"), BinaryExpression::create<BinaryOperation::REM>)
			)
		);
		return parse_expression(operators.tuple);
	}
	Expression* parse_program() {
		parse_white_space();
		Expression* expression = parse_expression();
		parse_white_space();
		if (parse(peek(any_char))) {
			error("unexpected character at end of program");
		}
		return expression;
	}
	MoebiusParser(const SourceFile* file): Parser(file) {}
public:
	static Expression* parse_program(const char* path) {
		SourceFile file(path);
		MoebiusParser parser(&file);
		return parser.parse_program();
	}
};
