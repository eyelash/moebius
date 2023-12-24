#pragma once

#include "common.hpp"
#include "printer.hpp"
#include "ast.hpp"
#include <map>
#include <cstdlib>

class ParseContext {
	const SourceFile* file;
	const char* position;
public:
	ParseContext(const SourceFile* file): file(file), position(file->begin()) {}
	constexpr ParseContext(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr bool operator <(const ParseContext& rhs) const {
		return position < rhs.position;
	}
	constexpr char operator *() const {
		return *position;
	}
	ParseContext& operator ++() {
		++position;
		return *this;
	}
	constexpr StringView operator -(const ParseContext& start) const {
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
	bool parse(ParseContext& context) const {
		if (context && f(*context)) {
			++context;
			return true;
		}
		return false;
	}
};

class StringParser {
	StringView s;
public:
	constexpr StringParser(const StringView& s): s(s) {}
	bool parse(ParseContext& context) const {
		ParseContext copy = context;
		for (char c: s) {
			if (!(copy && *copy == c)) {
				return false;
			}
			++copy;
		}
		context = copy;
		return true;
	}
};

template <class P0, class P1> class SequenceParser {
	P0 p0;
	P1 p1;
public:
	constexpr SequenceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
	bool parse(ParseContext& context) const {
		ParseContext copy = context;
		if (!p0.parse(copy)) {
			return false;
		}
		if (!p1.parse(copy)) {
			return false;
		}
		context = copy;
		return true;
	}
};

template <class P0, class P1> class ChoiceParser {
	P0 p0;
	P1 p1;
public:
	constexpr ChoiceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
	bool parse(ParseContext& context) const {
		if (p0.parse(context)) {
			return true;
		}
		if (p1.parse(context)) {
			return true;
		}
		return false;
	}
};

template <class P> class RepeatParser {
	P p;
public:
	constexpr RepeatParser(P p): p(p) {}
	bool parse(ParseContext& context) const {
		while (p.parse(context)) {}
		return true;
	}
};

template <class P> class NotParser {
	P p;
public:
	constexpr NotParser(P p): p(p) {}
	bool parse(ParseContext& context) const {
		ParseContext copy = context;
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
	bool parse(ParseContext& context) const {
		ParseContext copy = context;
		if (p.parse(copy)) {
			return true;
		}
		return false;
	}
};

template <class P, class = bool> struct is_parser: std::false_type {};
template <class P> struct is_parser<P, decltype(std::declval<P>().parse(std::declval<ParseContext&>()))>: std::true_type {};

template <class F, class = bool> struct is_char_class: std::false_type {};
template <class F> struct is_char_class<F, decltype(std::declval<F>()(std::declval<char>()))>: std::true_type {};

class Parser {
	ParseContext context;
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
		const ParseContext start = context;
		if (get_parser(p).parse(context)) {
			return context - start;
		}
		else {
			return StringView();
		}
	}
	Parser(const SourceFile* file): context(file) {}
	Parser(const ParseContext& context): context(context) {}
	const char* get_path() const {
		return context.get_path();
	}
	std::size_t get_position() const {
		return context.get_position();
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
