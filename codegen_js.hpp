#pragma once

#include "ast.hpp"

class CodegenJS: public Visitor<Variable> {
	static StringView print_operator(BinaryOperation operation) {
		switch (operation) {
			case BinaryOperation::ADD: return "+";
			case BinaryOperation::SUB: return "-";
			case BinaryOperation::MUL: return "*";
			case BinaryOperation::DIV: return "/";
			case BinaryOperation::REM: return "%";
			case BinaryOperation::EQ: return "==";
			case BinaryOperation::NE: return "!=";
			case BinaryOperation::LT: return "<";
			case BinaryOperation::LE: return "<=";
			case BinaryOperation::GT: return ">";
			case BinaryOperation::GE: return ">=";
			default: return StringView();
		}
	}
	class FunctionTable {
		std::map<const Function*, std::size_t> functions;
	public:
		std::size_t look_up(const Function* function) {
			auto iterator = functions.find(function);
			if (iterator != functions.end()) {
				return iterator->second;
			}
			const std::size_t index = functions.size();
			functions[function] = index;
			return index;
		}
	};
	FunctionTable& function_table;
	IndentPrinter& printer;
	std::size_t variable = 1;
	std::map<const Expression*, Variable> cache;
	Variable next_variable() {
		return Variable(variable++);
	}
	CodegenJS(FunctionTable& function_table, IndentPrinter& printer): function_table(function_table), printer(printer) {}
	Variable evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
		return cache[block.get_result()];
	}
public:
	Variable visit_number(const Number& number) override {
		const Variable result = next_variable();
		printer.println_indented(format("const % = %;", result, print_number(number.get_value())));
		return result;
	}
	Variable visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Variable left = cache[binary_expression.get_left()];
		const Variable right = cache[binary_expression.get_right()];
		const Variable result = next_variable();
		printer.println_indented(format("const % = % % % | 0;", result, left, print_operator(binary_expression.get_operation()), right));
		return result;
	}
	Variable visit_if(const If& if_) override {
		const Variable condition = cache[if_.get_condition()];
		const Variable result = next_variable();
		printer.println_indented(format("let %;", result));
		printer.println_indented(format("if (%) {", condition));
		printer.increase_indentation();
		const Variable then_result = evaluate(if_.get_then_block());
		printer.println_indented(format("% = %;", result, then_result));
		printer.decrease_indentation();
		printer.println_indented("} else {");
		printer.increase_indentation();
		const Variable else_result = evaluate(if_.get_else_block());
		printer.println_indented(format("% = %;", result, else_result));
		printer.decrease_indentation();
		printer.println_indented("}");
		return result;
	}
	Variable visit_tuple(const Tuple& tuple) override {
		const Variable result = next_variable();
		printer.println_indented(print_functor([&](auto& printer) {
			printer.print(format("const % = [", result));
			for (std::size_t i = 0; i < tuple.get_expressions().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(cache[tuple.get_expressions()[i]]);
			}
			printer.print("];");
		}));
		return result;
	}
	Variable visit_tuple_access(const TupleAccess& tuple_access) override {
		const Variable tuple = cache[tuple_access.get_tuple()];
		const Variable result = next_variable();
		printer.println_indented(format("const % = %[%];", result, tuple, print_number(tuple_access.get_index())));
		return result;
	}
	Variable visit_struct(const Struct& struct_) override {
		return next_variable();
	}
	Variable visit_struct_access(const StructAccess& struct_access) override {
		return next_variable();
	}
	Variable visit_closure(const Closure& closure) override {
		return next_variable();
	}
	Variable visit_closure_access(const ClosureAccess& closure_access) override {
		return next_variable();
	}
	Variable visit_argument(const Argument& argument) override {
		return Variable(argument.get_index());
	}
	Variable visit_call(const Call& call) override {
		const std::size_t new_index = function_table.look_up(call.get_function());
		const Variable result = next_variable();
		printer.println_indented(print_functor([&](auto& printer) {
			printer.print(format("const % = f%(", result, print_number(new_index)));
			for (std::size_t i = 0; i < call.get_arguments().size(); ++i) {
				if (i > 0) printer.print(", ");
				printer.print(cache[call.get_arguments()[i]]);
			}
			printer.print(");");
		}));
		return result;
	}
	Variable visit_intrinsic(const Intrinsic& intrinsic) override {
		const Variable result = next_variable();
		if (intrinsic.name_equals("putChar")) {
			const Variable argument = cache[intrinsic.get_arguments()[0]];
			printer.println_indented(format("const s = String.fromCharCode(%);", argument));
			printer.println_indented("document.body.appendChild(s === '\\n' ? document.createElement('br') : document.createTextNode(s));");
			printer.println_indented(format("const % = null;", result));
		}
		else if (intrinsic.name_equals("getChar")) {
			// TODO
		}
		return result;
	}
	Variable visit_bind(const Bind& bind) override {
		const Variable result = next_variable();
		printer.println_indented(format("const % = null;", result));
		return result;
	}
	static void codegen(const Program& program, const char* path) {
		FunctionTable function_table;
		IndentPrinter printer(std::cout);
		printer.println_indented("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><script>");
		printer.println_indented("window.addEventListener('load', main);");
		{
			printer.println_indented("function main() {");
			printer.increase_indentation();
			const std::size_t index = function_table.look_up(program.get_main_function());
			printer.println_indented(format("f%();", print_number(index)));
			printer.decrease_indentation();
			printer.println_indented("}");
		}
		for (const Function* function: program) {
			const std::size_t index = function_table.look_up(function);
			const std::size_t arguments = function->get_argument_types().size();
			printer.println_indented(print_functor([&](auto& printer) {
				printer.print(format("function f%(", print_number(index)));
				for (std::size_t i = 0; i < arguments; ++i) {
					if (i > 0) printer.print(", ");
					printer.print(format("v%", print_number(i)));
				}
				printer.print(") {");
			}));
			printer.increase_indentation();
			CodegenJS codegen(function_table, printer);
			codegen.variable = arguments;
			const Variable result = codegen.evaluate(function->get_block());
			printer.println_indented(format("return %;", result));
			printer.decrease_indentation();
			printer.println_indented("}");
		}
		printer.println_indented("</script></head><body></body></html>");
	}
};
