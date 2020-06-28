#pragma once

#include "ast.hpp"

class CodegenJS: public Visitor<std::size_t> {
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
	const std::size_t index;
	Printer& printer;
	std::size_t variable = 1;
	std::map<const Expression*, std::size_t> cache;
	CodegenJS(FunctionTable& function_table, std::size_t index, Printer& printer): function_table(function_table), index(index), printer(printer) {}
	std::size_t evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
		return cache[block.get_result()];
	}
public:
	std::size_t visit_number(const Number& number) override {
		const std::size_t result = variable++;
		printer.println(format("  const v% = %;", print_number(result), print_number(number.get_value())));
		return result;
	}
	std::size_t visit_binary_expression(const BinaryExpression& binary_expression) override {
		const std::size_t left = cache[binary_expression.get_left()];
		const std::size_t right = cache[binary_expression.get_right()];
		const std::size_t result = variable++;
		printer.println(format("  const v% = (v% % v%) | 0;", print_number(result), print_number(left), print_operator(binary_expression.get_operation()), print_number(right)));
		return result;
	}
	std::size_t visit_if(const If& if_) override {
		const std::size_t condition = cache[if_.get_condition()];
		const std::size_t result = variable++;
		printer.println(format("  let v%;", print_number(result)));
		printer.println(format("  if (v%) {", print_number(condition)));
		const std::size_t then_result = evaluate(if_.get_then_block());
		printer.println(format("  v% = v%;", print_number(result), print_number(then_result)));
		printer.println("  } else {");
		const std::size_t else_result = evaluate(if_.get_else_block());
		printer.println(format("  v% = v%;", print_number(result), print_number(else_result)));
		printer.println("  }");
		return result;
	}
	std::size_t visit_closure(const Closure& closure) override {
		std::vector<std::size_t> elements;
		for (const Expression* element: closure.get_environment_expressions()) {
			elements.push_back(cache[element]);
		}
		const std::size_t result = variable++;
		printer.print(format("  const v% = [", print_number(result)));
		for (const std::size_t element: elements) {
			printer.print(format("v%,", print_number(element)));
		}
		printer.println("];");
		return result;
	}
	std::size_t visit_closure_access(const ClosureAccess& closure_access) override {
		const std::size_t closure = cache[closure_access.get_closure()];
		const std::size_t result = variable++;
		printer.println(format("  const v% = v%[%];", print_number(result), print_number(closure), print_number(closure_access.get_index())));
		return result;
	}
	std::size_t visit_argument(const Argument& argument) override {
		return argument.get_index();
	}
	std::size_t visit_call(const Call& call) override {
		std::vector<std::size_t> arguments;
		for (const Expression* argument: call.get_arguments()) {
			arguments.push_back(cache[argument]);
		}
		const std::size_t new_index = function_table.look_up(call.get_function());
		const std::size_t result = variable++;
		printer.print(format("  const v% = f%(", print_number(result), print_number(new_index)));
		for (const std::size_t argument: arguments) {
			printer.print(format("v%,", print_number(argument)));
		}
		printer.println(");");
		return result;
	}
	std::size_t visit_intrinsic(const Intrinsic& intrinsic) override {
		if (intrinsic.name_equals("putChar")) {
			const std::size_t argument = cache[intrinsic.get_arguments()[0]];
			printer.println(format("  const s = String.fromCharCode(v%);", print_number(argument)));
			printer.println("  document.body.appendChild(s === '\\n' ? document.createElement('br') : document.createTextNode(s));");
			const std::size_t result = variable++;
			printer.println(format("  const v% = null;", print_number(result)));
			return result;
		}
		else if (intrinsic.name_equals("getChar")) {
			// TODO
		}
	}
	std::size_t visit_bind(const Bind& bind) override {}
	static void codegen(const Program& program, const char* path) {
		FunctionTable function_table;
		Printer printer(stdout);
		printer.println("<!DOCTYPE html><html><head><script>");
		printer.println("window.addEventListener('load', start);");
		{
			printer.println("function start() {");
			const std::size_t index = function_table.look_up(program.get_main_function());
			printer.println(format("  f%();", print_number(index)));
			printer.println("}");
		}
		for (const Function* function: program) {
			const std::size_t index = function_table.look_up(function);
			printer.print(format("function f%(", print_number(index)));
			const std::size_t arguments = function->get_argument_types().size();
			for (std::size_t i = 0; i < arguments; ++i) {
				printer.print(format("v%,", print_number(i)));
			}
			printer.println(") {");
			CodegenJS codegen(function_table, index, printer);
			codegen.variable = arguments;
			const std::size_t result = codegen.evaluate(function->get_block());
			printer.println(format("  return v%;", print_number(result)));
			printer.println("}");
		}
		printer.println("</script></head><body></body></html>");
	}
};
