#pragma once

#include "common.hpp"
#include "printer.hpp"
#include "ast.hpp"
#include <variant>
#include "parsley/parser.hpp"
#include "parsley/pratt.hpp"

template <class T> using ParseResult = std::variant<std::conditional_t<std::is_void_v<T>, std::monostate, T>, Error<std::string>>;

namespace moebius_parser {

using namespace parser;

constexpr bool any_char(char c) {
	return true;
}

constexpr bool white_space(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr bool numeric(char c) {
	return c >= '0' && c <= '9';
}

constexpr bool alphabetic(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

constexpr bool alphanumeric(char c) {
	return alphabetic(c) || numeric(c);
}

constexpr auto keyword(const StringView& s) {
	return sequence(ignore(s), not_(alphanumeric));
}

constexpr auto expect_keyword(const StringView& s) {
	return expect(s);
}

constexpr auto parse_comment = choice(
	sequence("//",
		zero_or_more(sequence(not_("\n"), any_char))
	),
	sequence("/*",
		zero_or_more(sequence(not_("*/"), any_char)),
		expect("*/")
	)
);
constexpr auto parse_white_space = ignore(sequence(
	zero_or_more(white_space),
	zero_or_more(sequence(parse_comment,
		zero_or_more(white_space)
	))
));

class IfCollector {
	Reference<Expression> condition;
	Reference<Expression> then_expression;
	Reference<Expression> else_expression;
public:
	struct ConditionTag {};
	struct ThenTag {};
	struct ElseTag {};
	void push(Reference<Expression>&& expression, ConditionTag) {
		condition = std::move(expression);
	}
	void push(Reference<Expression>&& expression, ThenTag) {
		then_expression = std::move(expression);
	}
	void push(Reference<Expression>&& expression, ElseTag) {
		else_expression = std::move(expression);
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new If(std::move(condition), std::move(then_expression), std::move(else_expression)));
	}
};

class FalseCollector {
public:
	template <class C> void retrieve(const C& callback) {
		callback.push(new IntLiteral(0));
	}
};

class TrueCollector {
public:
	template <class C> void retrieve(const C& callback) {
		callback.push(new IntLiteral(1));
	}
};

class IntLiteralCollector {
	std::int32_t number = 0;
public:
	void push(char c) {
		number *= 10;
		number += c - '0';
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(new IntLiteral(number));
	}
};

class ExpressionCollector {
	Reference<Expression> expression;
public:
	void push(Reference<Expression>&& expression) {
		this->expression = std::move(expression);
	}
	void push(Reference<Expression>&& expression, BinaryOperation operation) {
		this->expression = new BinaryExpression(operation, std::move(this->expression), std::move(expression));
	}
	template <class C> void retrieve(const C& callback) {
		callback.push(std::move(expression));
	}
};

template <BinaryOperation operation> class OperationMapper {
public:
	template <class C, class... A> static void map(const C& callback, A&&... a) {
		callback.push(std::forward<A>(a)..., operation);
	}
};

template <class P> constexpr auto operator_(P p) {
	return sequence(parse_white_space, ignore(p), parse_white_space);
}

template <BinaryOperation operation, class P> constexpr auto infix_ltr(P p) {
	return infix_ltr<OperationMapper<operation>>(operator_(p));
}

struct parse_expression {
	static constexpr auto last = choice(
		sequence(
			ignore('('),
			parse_white_space,
			reference<parse_expression>(),
			parse_white_space,
			expect(")")
		),
		collect<IfCollector>(sequence(
			keyword("if"),
			parse_white_space,
			expect("("),
			parse_white_space,
			tag<IfCollector::ConditionTag>(reference<parse_expression>()),
			parse_white_space,
			expect(")"),
			parse_white_space,
			tag<IfCollector::ThenTag>(reference<parse_expression>()),
			parse_white_space,
			expect_keyword("else"),
			parse_white_space,
			tag<IfCollector::ElseTag>(reference<parse_expression>())
		)),
		collect<FalseCollector>(keyword("false")),
		collect<TrueCollector>(keyword("true")),
		collect<IntLiteralCollector>(sequence(
			and_(numeric),
			zero_or_more(numeric)
		)),
		error("expected an expression")
	);
	static constexpr auto parser = pratt<ExpressionCollector>(
		pratt_level(
			infix_ltr<BinaryOperation::EQ>("=="),
			infix_ltr<BinaryOperation::NE>("!=")
		),
		pratt_level(
			infix_ltr<BinaryOperation::LT>(sequence('<', not_('='))),
			infix_ltr<BinaryOperation::LE>("<="),
			infix_ltr<BinaryOperation::GT>(sequence('>', not_('='))),
			infix_ltr<BinaryOperation::GE>(">=")
		),
		pratt_level(
			infix_ltr<BinaryOperation::ADD>('+'),
			infix_ltr<BinaryOperation::SUB>('-')
		),
		pratt_level(
			infix_ltr<BinaryOperation::MUL>('*'),
			infix_ltr<BinaryOperation::DIV>('/'),
			infix_ltr<BinaryOperation::REM>('%')
		),
		pratt_level(
			terminal(last)
		)
	);
};

constexpr auto program = sequence(
	parse_white_space,
	reference<parse_expression>(),
	parse_white_space,
	choice(
		not_(any_char),
		error("unexpected character at end of program")
	)
);

ParseResult<Reference<Expression>> parse_program(const char* path) {
	auto source = read_file(path);
	Context context(source);
	Reference<Expression> expression;
	const Result result = parse_impl(program, context, GetValueCallback<Reference<Expression>>(expression));
	if (result == ERROR) {
		return Error(path, context.get_position(), context.get_error());
	}
	return expression;
}

}
