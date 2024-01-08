#pragma once

#include "ast.hpp"

class Interpreter: public Visitor<std::int32_t> {
public:
	std::int32_t visit_int_literal(const IntLiteral& int_literal) override {
		return int_literal.get_value();
	}
	std::int32_t visit_binary_expression(const BinaryExpression& binary_expression) override {
		const std::int32_t left = visit(*this, binary_expression.get_left());
		const std::int32_t right = visit(*this, binary_expression.get_right());
		switch (binary_expression.get_operation()) {
		case BinaryOperation::ADD:
			return left + right;
		case BinaryOperation::SUB:
			return left - right;
		case BinaryOperation::MUL:
			return left * right;
		case BinaryOperation::DIV:
			return left / right;
		case BinaryOperation::REM:
			return left % right;
		case BinaryOperation::EQ:
			return left == right;
		case BinaryOperation::NE:
			return left != right;
		case BinaryOperation::LT:
			return left < right;
		case BinaryOperation::LE:
			return left <= right;
		case BinaryOperation::GT:
			return left > right;
		case BinaryOperation::GE:
			return left >= right;
		default:
			return 0;
		}
	}
	std::int32_t visit_if(const If& if_) override {
		const std::int32_t condition = visit(*this, if_.get_condition());
		if (condition) {
			return visit(*this, if_.get_then_expression());
		}
		else {
			return visit(*this, if_.get_else_expression());
		}
	}
	static void interpret_program(const Expression* program) {
		Interpreter interpreter;
		const std::int32_t result = visit(interpreter, program);
		print(ln(print_number(result)));
	}
};
