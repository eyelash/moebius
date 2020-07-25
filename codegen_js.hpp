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
	const std::size_t index;
	FilePrinter& printer;
	std::size_t variable = 1;
	std::map<const Expression*, Variable> cache;
	std::size_t indentation = 1;
	Variable next_variable() {
		return Variable(variable++);
	}
	template <class T> Indent<T> indent(const T& t) {
		return Indent<T>(t, indentation);
	}
	CodegenJS(FunctionTable& function_table, std::size_t index, FilePrinter& printer): function_table(function_table), index(index), printer(printer) {}
	Variable evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
		return cache[block.get_result()];
	}
public:
	Variable visit_number(const Number& number) override {
		const Variable result = next_variable();
		printer.println(indent(format("const % = %;", result, print_number(number.get_value()))));
		return result;
	}
	Variable visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Variable left = cache[binary_expression.get_left()];
		const Variable right = cache[binary_expression.get_right()];
		const Variable result = next_variable();
		printer.println(indent(format("const % = (% % %) | 0;", result, left, print_operator(binary_expression.get_operation()), right)));
		return result;
	}
	Variable visit_if(const If& if_) override {
		const Variable condition = cache[if_.get_condition()];
		const Variable result = next_variable();
		printer.println(indent(format("let %;", result)));
		printer.println(indent(format("if (%) {", condition)));
		++indentation;
		const Variable then_result = evaluate(if_.get_then_block());
		printer.println(indent(format("% = %;", result, then_result)));
		--indentation;
		printer.println(indent("} else {"));
		++indentation;
		const Variable else_result = evaluate(if_.get_else_block());
		printer.println(indent(format("% = %;", result, else_result)));
		--indentation;
		printer.println(indent("}"));
		return result;
	}
	Variable visit_closure(const Closure& closure) override {
		std::vector<Variable> elements;
		for (const Expression* element: closure.get_environment_expressions()) {
			elements.push_back(cache[element]);
		}
		const Variable result = next_variable();
		printer.print(indent(format("const % = [", result)));
		for (const Variable element: elements) {
			printer.print(format("%,", element));
		}
		printer.println("];");
		return result;
	}
	Variable visit_closure_access(const ClosureAccess& closure_access) override {
		const Variable closure = cache[closure_access.get_closure()];
		const Variable result = next_variable();
		printer.println(indent(format("const % = %[%];", result, closure, print_number(closure_access.get_index()))));
		return result;
	}
	Variable visit_argument(const Argument& argument) override {
		return Variable(argument.get_index());
	}
	Variable visit_call(const Call& call) override {
		std::vector<Variable> arguments;
		for (const Expression* argument: call.get_arguments()) {
			arguments.push_back(cache[argument]);
		}
		const std::size_t new_index = function_table.look_up(call.get_function());
		const Variable result = next_variable();
		printer.print(indent(format("const % = f%(", result, print_number(new_index))));
		for (const Variable argument: arguments) {
			printer.print(format("%,", argument));
		}
		printer.println(");");
		return result;
	}
	Variable visit_intrinsic(const Intrinsic& intrinsic) override {
		const Variable result = next_variable();
		if (intrinsic.name_equals("putChar")) {
			const Variable argument = cache[intrinsic.get_arguments()[0]];
			printer.println(indent(format("const s = String.fromCharCode(%);", argument)));
			printer.println(indent("document.body.appendChild(s === '\\n' ? document.createElement('br') : document.createTextNode(s));"));
			printer.println(indent(format("const % = null;", result)));
		}
		else if (intrinsic.name_equals("getChar")) {
			// TODO
		}
		return result;
	}
	Variable visit_bind(const Bind& bind) override {
		const Variable result = next_variable();
		printer.println(indent(format("const % = null;", result)));
		return result;
	}
	static void codegen(const Program& program, const char* path) {
		FunctionTable function_table;
		FilePrinter printer(stdout);
		printer.println("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><script>");
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
			const Variable result = codegen.evaluate(function->get_block());
			printer.println(format("  return %;", result));
			printer.println("}");
		}
		printer.println("</script></head><body></body></html>");
	}
};
