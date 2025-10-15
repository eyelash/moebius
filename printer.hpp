#pragma once

#include "ast.hpp"
#include "parsley/printer.hpp"

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

template <class P, class C> void print_message(printer::Context& context, const char* path, std::size_t source_position, const C& color, const char* severity, const P& p) {
	using namespace printer;
	if (path == nullptr) {
		print_message(context, color, severity, p);
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

		print_impl(format("%:%:%: ", path, print_number(line_number), print_number(column)), context);
		print_message(context, color, severity, p);

		c = line_start;
		while (c < end && *c != '\n') {
			context.print(*c);
			++c;
		}
		context.print('\n');

		c = line_start;
		while (c < position) {
			context.print(*c == '\t' ? '\t' : ' ');
			++c;
		}
		print_impl(bold(color('^')), context);
		context.print('\n');
	}
}

template <class P> class Error {
public:
	const char* path;
	std::size_t source_position;
	P p;
	constexpr Error(const char* path, std::size_t source_position, P p): path(path), source_position(source_position), p(p) {}
};
class ErrorPrinter {
	const Error<std::string>* error;
public:
	ErrorPrinter(const Error<std::string>* error): error(error) {}
	void print(printer::Context& context) const {
		using namespace printer;
		print_message(context, error->path, error->source_position, red, "error", get_printer(error->p));
	}
};

class PrintExpression {
	static const char* print_operation(BinaryOperation operation) {
		switch (operation) {
		case BinaryOperation::ADD:
			return "+";
		case BinaryOperation::SUB:
			return "-";
		case BinaryOperation::MUL:
			return "*";
		case BinaryOperation::DIV:
			return "/";
		case BinaryOperation::REM:
			return "%";
		case BinaryOperation::EQ:
			return "==";
		case BinaryOperation::NE:
			return "!=";
		case BinaryOperation::LT:
			return "<";
		case BinaryOperation::LE:
			return "<=";
		case BinaryOperation::GT:
			return ">";
		case BinaryOperation::GE:
			return ">=";
		default:
			return "";
		}
	}
	const Expression* expression;
public:
	PrintExpression(const Expression* expression): expression(expression) {}
	void print(printer::Context& context) const {
		using namespace printer;
		class PrintExpressionVisitor: public Visitor<void> {
			printer::Context& context;
		public:
			PrintExpressionVisitor(printer::Context& context): context(context) {}
			void visit_int_literal(const IntLiteral& int_literal) override {
				print_impl(print_number(int_literal.get_value()), context);
			}
			void visit_binary_expression(const BinaryExpression& binary_expression) override {
				print_impl(format("(% % %)", PrintExpression(binary_expression.get_left()), print_operation(binary_expression.get_operation()), PrintExpression(binary_expression.get_right())), context);
			}
			void visit_if(const If& if_) override {
				print_impl(ln(format("if (%)", PrintExpression(if_.get_condition()))), context);
				print_impl(indented(ln(PrintExpression(if_.get_then_expression()))), context);
				print_impl(ln("else"), context);
				print_impl(indented(ln(PrintExpression(if_.get_else_expression()))), context);
			}
		};
		PrintExpressionVisitor visitor(context);
		expression->accept(visitor);
	}
};
