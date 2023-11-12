#pragma once

#include "ast.hpp"

class Interpreter: public Visitor<std::int32_t> {
public:
	std::int32_t visit_int_literal(const IntLiteral& int_literal) override {
		return int_literal.get_value();
	}
	static void interpret_program(const Expression* program) {
		Interpreter interpreter;
		const std::int32_t result = visit(interpreter, program);
		Printer printer;
		printer.println(print_number(result));
	}
};
