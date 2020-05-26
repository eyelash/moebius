#pragma once

#include "ast.hpp"
#include "assembler.hpp"
#include <cstdlib>

// type checking and monomorphization
class Pass1: public Visitor {
	static bool equals(const Type* type1, const Type* type2) {
		const int id1 = type1->get_id();
		const int id2 = type2->get_id();
		if (id1 != id2) {
			return false;
		}
		if (id1 == FunctionType::id) {
			const FunctionType* function_type1 = static_cast<const FunctionType*>(type1);
			const FunctionType* function_type2 = static_cast<const FunctionType*>(type2);
			if (function_type1->get_function() != function_type2->get_function()) {
				return false;
			}
			return equals(function_type1->get_environment_types(), function_type2->get_environment_types());
		}
		return true;
	}
	static bool equals(const std::vector<const Type*>& types1, const std::vector<const Type*>& types2) {
		if (types1.size() != types2.size()) {
			return false;
		}
		for (std::size_t i = 0; i < types1.size(); ++i) {
			if (!equals(types1[i], types2[i])) {
				return false;
			}
		}
		return true;
	}
	static StringView print_type(const Type* type) {
		switch (type->get_id()) {
			case NumberType::id: return "Number";
			case FunctionType::id: return "Function";
			case VoidType::id: return "Void";
			default: return StringView();
		}
	}
	template <class T> [[noreturn]] void error(const Expression* expression, const T& t) {
		Printer printer(stderr);
		expression->get_position().print_error(printer, t);
		std::exit(EXIT_FAILURE);
	}
	template <class T> [[noreturn]] void error(const T& t) {
		Printer printer(stderr);
		printer.print(bold(red("error: ")));
		printer.print(t);
		printer.print('\n');
		std::exit(EXIT_FAILURE);
	}
	struct FunctionTableEntry {
		const Function* old_function;
		std::vector<const Type*> argument_types;
		std::vector<const Type*> environment_types;
		const Function* new_function = nullptr;
		FunctionTableEntry(const Function* old_function, const std::vector<const Type*>& argument_types, const std::vector<const Type*>& environment_types): old_function(old_function), argument_types(argument_types), environment_types(environment_types) {}
		const Type* look_up(std::size_t index) const {
			if (index < argument_types.size()) {
				return argument_types[index];
			}
			index -= argument_types.size();
			if (index < environment_types.size()) {
				return environment_types[index];
			}
			return nullptr;
		}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::size_t recursion = -1;
		std::size_t look_up(const Function* old_function, const std::vector<const Type*>& argument_types, const std::vector<const Type*>& environment_types) const {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				const FunctionTableEntry& entry = functions[index];
				if (entry.old_function == old_function && equals(entry.argument_types, argument_types) && equals(entry.environment_types, environment_types)) {
					return index;
				}
			}
			return index;
		}
		void add_entry(const Function* old_function, const std::vector<const Type*>& argument_types, const std::vector<const Type*>& environment_types) {
			functions.emplace_back(old_function, argument_types, environment_types);
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
		void clear(std::size_t index) {
			functions.erase(functions.begin() + index + 1, functions.end());
		}
	};
	const Expression* expression;
	FunctionTable& function_table;
	std::size_t index;
	Pass1(FunctionTable& function_table, std::size_t index): expression(nullptr), function_table(function_table), index(index) {}
	const Expression* evaluate(const Expression* expression) {
		this->expression = nullptr;
		expression->accept(this);
		const Expression* result = this->expression;
		this->expression = nullptr;
		return result;
	}
public:
	void visit_number(const Number* number) override {
		expression = number;
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const Expression* left = evaluate(binary_expression->get_left());
		const Expression* right = evaluate(binary_expression->get_right());
		if ((left->get_type_id() == NumberType::id || left->get_type_id() == NeverType::id) && (right->get_type_id() == NumberType::id || right->get_type_id() == NeverType::id)) {
			expression = new BinaryExpression(binary_expression->get_operation(), left, right);
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	void visit_if(const If* if_) override {
		const Expression* condition = evaluate(if_->get_condition());
		if (condition->get_type_id() == NumberType::id || condition->get_type_id() == NeverType::id) {
			const Expression* then_expression = evaluate(if_->get_then_expression());
			const Expression* else_expression = evaluate(if_->get_else_expression());
			if (equals(then_expression->get_type(), else_expression->get_type())) {
				expression = new If(condition, then_expression, else_expression, then_expression->get_type());
			}
			else if (then_expression->get_type_id() == NeverType::id) {
				expression = new If(condition, then_expression, else_expression, else_expression->get_type());
			}
			else if (else_expression->get_type_id() == NeverType::id) {
				expression = new If(condition, then_expression, else_expression, then_expression->get_type());
			}
			else {
				error(if_, "if and else branches must return values of the same type");
			}
		}
		else {
			error(if_, "type of condition must be a number");
		}
	}
	void visit_function(const Function* function) override {
		FunctionType* type = new FunctionType(function);
		Function* new_function = new Function(nullptr, type);
		for (const Expression* expression: function->get_environment_expressions()) {
			const Expression* new_expression = evaluate(expression);
			type->add_environment_type(new_expression->get_type());
			new_function->add_environment_expression(new_expression);
		}
		expression = new_function;
	}
	void visit_argument(const Argument* argument) override {
		const Type* type = function_table[index].look_up(argument->get_index());
		expression = new Argument(argument->get_index(), type);
	}
	void visit_call(const Call* call) override {
		const Expression* object = evaluate(call->get_object());
		if (object->get_type_id() != FunctionType::id) {
			error("call to a value that is not a function");
		}
		const FunctionType* type = static_cast<const FunctionType*>(object->get_type());
		const Function* old_function = type->get_function();
		if (call->get_arguments().size() != old_function->get_arguments()) {
			error(format("call with % arguments to a function that accepts % arguments", print_number(call->get_arguments().size()), print_number(old_function->get_arguments())));
		}
		std::vector<const Type*> argument_types;
		Call* new_call = new Call(object);
		for (const Expression* argument: call->get_arguments()) {
			const Expression* new_argument = evaluate(argument);
			argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}

		const std::size_t new_index = function_table.look_up(old_function, argument_types, type->get_environment_types());
		if (new_index == function_table.size()) {
			function_table.add_entry(old_function, argument_types, type->get_environment_types());
			Pass1 pass1(function_table, new_index);
			const Expression* new_expression = pass1.evaluate(old_function->get_expression());
			Function* new_function = new Function(new_expression, new_expression->get_type());
			function_table[new_index].new_function = new_function;
			if (function_table.recursion == new_index) {
				// reevaluate the expression in case of recursion
				function_table.recursion = -1;
				function_table.clear(new_index);
				new_expression = pass1.evaluate(old_function->get_expression());
				new_function->set_expression(new_expression);
				new_function->set_type(new_expression->get_type());
			}
			new_call->set_type(new_expression->get_type());
			new_call->set_function(new_function);
		}
		else {
			// detect recursion
			if (function_table[new_index].new_function == nullptr) {
				if (new_index < function_table.recursion) {
					function_table.recursion = new_index;
				}
				new_call->set_type(new NeverType());
			}
			else {
				new_call->set_type(function_table[new_index].new_function->get_type());
				new_call->set_function(function_table[new_index].new_function);
			}
		}
		expression = new_call;
	}
	void visit_intrinsic(const Intrinsic* intrinsic) override {
		if (intrinsic->get_name() == "putChar") {
			const Expression* argument = evaluate(intrinsic->get_arguments()[0]);
			if (argument->get_type_id() != NumberType::id) {
				error("argument of putChar must be a number");
			}
			expression = intrinsic;
		}
		else if (intrinsic->get_name() == "getChar") {
			expression = intrinsic;
		}
	}
	static const Expression* run(const Expression* expression) {
		FunctionTable function_table;
		Pass1 pass1(function_table, 0);
		return pass1.evaluate(expression);
	}
};

class CodegenX86: public Visitor {
	using A = Assembler;
	using Jump = typename A::Jump;
	static std::uint32_t get_type_size(const Type* type) {
		switch (type->get_id()) {
			case NumberType::id:
				return 4;
			case FunctionType::id: {
				int size = 0;
				for (const Type* type: static_cast<const FunctionType*>(type)->get_environment_types()) {
					size += get_type_size(type);
				}
				return size;
			}
			default:
				return 0;
		}
	}
	struct DeferredCall {
		Jump jump;
		std::size_t function_index;
		DeferredCall(const Jump& jump, std::size_t function_index): jump(jump), function_index(function_index) {}
	};
	struct FunctionTableEntry {
		const Function* function;
		std::vector<const Type*> argument_types;
		std::vector<const Type*> environment_types;
		std::uint32_t input_size;
		std::uint32_t output_size;
		std::size_t position;
		FunctionTableEntry(const Function* function, const std::vector<const Type*>& argument_types, const std::vector<const Type*>& environment_types): function(function), argument_types(argument_types), environment_types(environment_types) {
			input_size = 0;
			for (const Type* type: argument_types) {
				input_size += get_type_size(type);
			}
			for (const Type* type: environment_types) {
				input_size += get_type_size(type);
			}
			output_size = get_type_size(function->get_expression()->get_type());
		}
		const Type* look_up(std::size_t index, std::uint32_t& location) const {
			location = std::max(input_size, output_size);
			for (std::size_t i = 0; i < argument_types.size(); ++i) {
				location -= get_type_size(argument_types[i]);
				if (i == index) {
					return argument_types[i];
				}
			}
			index -= argument_types.size();
			for (std::size_t i = 0; i < environment_types.size(); ++i) {
				location -= get_type_size(environment_types[i]);
				if (i == index) {
					return environment_types[i];
				}
			}
			return nullptr;
		}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::vector<DeferredCall> deferred_calls;
		std::size_t done = 0;
		std::size_t look_up(const Function* function, const std::vector<const Type*>& argument_types, const std::vector<const Type*>& environment_types) {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				if (functions[index].function == function) {
					return index;
				}
			}
			functions.emplace_back(function, argument_types, environment_types);
			return index;
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
	};
	FunctionTable& function_table;
	const std::size_t index;
	A& assembler;
	CodegenX86(FunctionTable& function_table, std::size_t index, A& assembler): function_table(function_table), index(index), assembler(assembler) {}
	void evaluate(const Expression* expression) {
		expression->accept(this);
	}
public:
	void visit_number(const Number* number) override {
		assembler.PUSH(number->get_value());
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const Expression* left = binary_expression->get_left();
		const Expression* right = binary_expression->get_right();
		evaluate(left);
		evaluate(right);
		assembler.POP(EBX);
		assembler.POP(EAX);
		switch (binary_expression->get_operation()) {
			case BinaryOperation::ADD:
				assembler.ADD(EAX, EBX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::SUB:
				assembler.SUB(EAX, EBX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::MUL:
				assembler.IMUL(EBX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::DIV:
				assembler.CDQ();
				assembler.IDIV(EBX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::REM:
				assembler.CDQ();
				assembler.IDIV(EBX);
				assembler.PUSH(EDX);
				break;
			case BinaryOperation::EQ:
				assembler.CMP(EAX, EBX);
				assembler.SETE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::NE:
				assembler.CMP(EAX, EBX);
				assembler.SETNE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::LT:
				assembler.CMP(EAX, EBX);
				assembler.SETL(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::LE:
				assembler.CMP(EAX, EBX);
				assembler.SETLE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::GT:
				assembler.CMP(EAX, EBX);
				assembler.SETG(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.PUSH(EAX);
				break;
			case BinaryOperation::GE:
				assembler.CMP(EAX, EBX);
				assembler.SETGE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.PUSH(EAX);
				break;
		}
	}
	void visit_if(const If* if_) override {
		const Expression* condition = if_->get_condition();
		evaluate(condition);
		assembler.POP(EAX);
		assembler.CMP(EAX, 0);
		const Jump jump_else = assembler.JE();
		assembler.comment("if");
		evaluate(if_->get_then_expression());
		const Jump jump_end = assembler.JMP();
		assembler.comment("else");
		jump_else.set_target(assembler.get_position());
		evaluate(if_->get_else_expression());
		assembler.comment("end");
		jump_end.set_target(assembler.get_position());
	}
	void visit_function(const Function* function) override {
		for (const Expression* expression: function->get_environment_expressions()) {
			evaluate(expression);
		}
	}
	void visit_argument(const Argument* argument) override {
		std::uint32_t location;
		const Type* type = function_table[index].look_up(argument->get_index(), location);
		const std::uint32_t size = get_type_size(type);
		for (std::uint32_t i = 0; i < size; i += 4) {
			assembler.MOV(EAX, PTR(EBP, 8 + location + size - 4 - i));
			assembler.PUSH(EAX);
		}
	}
	void visit_call(const Call* call) override {
		std::vector<const Type*> argument_types;
		for (const Expression* argument: call->get_arguments()) {
			evaluate(argument);
			argument_types.push_back(argument->get_type());
		}
		const Expression* object = call->get_object();
		evaluate(object);
		// assert(object->get_type_id() == FunctionType::id);
		const FunctionType* type = static_cast<const FunctionType*>(object->get_type());
		const std::size_t new_index = function_table.look_up(call->get_function(), argument_types, type->get_environment_types());
		const std::uint32_t input_size = function_table[new_index].input_size;
		const std::uint32_t output_size = function_table[new_index].output_size;
		if (output_size > input_size) {
			// negative in order to grow the stack
			assembler.ADD(ESP, input_size - output_size);
		}
		Jump jump = assembler.CALL();
		function_table.deferred_calls.emplace_back(jump, new_index);
		if (output_size < input_size) {
			assembler.ADD(ESP, input_size - output_size);
		}
	}
	void visit_intrinsic(const Intrinsic* intrinsic) override {
		if (intrinsic->get_name() == "putChar") {
			evaluate(intrinsic->get_arguments()[0]);
			assembler.comment("putChar");
			assembler.MOV(EAX, 0x04);
			assembler.MOV(EBX, 1); // stdout
			assembler.MOV(ECX, ESP);
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.POP(EAX);
		}
		else if (intrinsic->get_name() == "getChar") {
			assembler.comment("getChar");
			assembler.PUSH(0);
			assembler.MOV(EAX, 0x03);
			assembler.MOV(EBX, 0); // stdin
			assembler.MOV(ECX, ESP);
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.MOVZX(EAX, EAX);
		}
	}
	static void codegen(const Expression* expression, const char* path) {
		FunctionTable function_table;
		A assembler;
		{
			// the main function
			CodegenX86 codegen(function_table, 0, assembler);
			codegen.evaluate(expression);
			assembler.comment("exit");
			assembler.MOV(EAX, 0x01);
			assembler.MOV(EBX, 0);
			assembler.INT(0x80);
		}
		while (function_table.done < function_table.size()) {
			const std::size_t index = function_table.done;
			assembler.comment("function");
			function_table[index].position = assembler.get_position();
			assembler.PUSH(EBP);
			assembler.MOV(EBP, ESP);
			assembler.comment("--");
			CodegenX86 codegen(function_table, index, assembler);
			codegen.evaluate(function_table[index].function->get_expression());
			assembler.comment("--");
			const std::uint32_t output_size = function_table[index].output_size;
			const std::uint32_t size = std::max(function_table[index].input_size, output_size);
			for (std::uint32_t i = 0; i < output_size; i += 4) {
				assembler.POP(EAX);
				assembler.MOV(PTR(EBP, 8 + size - output_size + i), EAX);
			}
			//assembler.ADD(ESP, output_size);
			assembler.MOV(ESP, EBP);
			assembler.POP(EBP);
			assembler.RET();
			++function_table.done;
		}
		for (const DeferredCall& deferred_call: function_table.deferred_calls) {
			const std::size_t target = function_table[deferred_call.function_index].position;
			deferred_call.jump.set_target(target);
		}
		assembler.write_file(path);
	}
};

class CodegenJS: public Visitor {
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
	struct FunctionTableEntry {
		const Function* function;
		std::size_t arguments;
		FunctionTableEntry(const Function* function, std::size_t arguments): function(function), arguments(arguments) {}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::size_t variable = 1;
		std::size_t done = 0;
		std::size_t look_up(const Function* function, std::size_t arguments) {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				if (functions[index].function == function) {
					return index;
				}
			}
			functions.emplace_back(function, arguments);
			return index;
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
	};
	std::size_t result;
	FunctionTable& function_table;
	const std::size_t index;
	Printer& printer;
	CodegenJS(FunctionTable& function_table, std::size_t index, Printer& printer): function_table(function_table), index(index), printer(printer) {}
	std::size_t evaluate(const Expression* expression) {
		result = 0;
		expression->accept(this);
		return result;
	}
public:
	void visit_number(const Number* number) override {
		result = function_table.variable++;
		printer.println(format("  let v% = %;", print_number(result), print_number(number->get_value())));
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const std::size_t left = evaluate(binary_expression->get_left());
		const std::size_t right = evaluate(binary_expression->get_right());
		result = function_table.variable++;
		printer.println(format("  let v% = v% % v%;", print_number(result), print_number(left), print_operator(binary_expression->get_operation()), print_number(right)));
	}
	void visit_if(const If* if_) override {
		const std::size_t condition = evaluate(if_->get_condition());
		const std::size_t result = function_table.variable++;
		printer.println(format("  let v%;", print_number(result)));
		printer.println(format("  if (v%) {", print_number(condition)));
		const std::size_t inner_then = evaluate(if_->get_then_expression());
		printer.println(format("  v% = v%;", print_number(result), print_number(inner_then)));
		printer.println("  } else {");
		const std::size_t inner_else = evaluate(if_->get_else_expression());
		printer.println(format("  v% = v%;", print_number(result), print_number(inner_else)));
		printer.println("  }");
		this->result = result;
	}
	void visit_function(const Function* function) override {
		std::vector<std::size_t> environment;
		for (const Expression* expression: function->get_environment_expressions()) {
			environment.push_back(evaluate(expression));
		}
		result = function_table.variable++;
		printer.print(format("  let v% = [", print_number(result)));
		for (const std::size_t value: environment) {
			printer.print(format("v%,", print_number(value)));
		}
		printer.println("];");
	}
	void visit_argument(const Argument* argument) override {
		result = function_table.variable++;
		const std::size_t argument_index = argument->get_index();
		if (argument_index < function_table[index].arguments) {
			printer.println(format("  let v% = a%;", print_number(result), print_number(argument_index)));
		}
		else {
			const std::size_t object_index = argument_index - function_table[index].arguments;
			printer.println(format("  let v% = obj[%];", print_number(result), print_number(object_index)));
		}
	}
	void visit_call(const Call* call) override {
		std::vector<std::size_t> arguments;
		for (const Expression* argument: call->get_arguments()) {
			arguments.push_back(evaluate(argument));
		}
		const std::size_t object = evaluate(call->get_object());
		const std::size_t new_index = function_table.look_up(call->get_function(), arguments.size());
		result = function_table.variable++;
		printer.print(format("  let v% = f%(v%", print_number(result), print_number(new_index), print_number(object)));
		for (const std::size_t argument: arguments) {
			printer.print(format(", v%", print_number(argument)));
		}
		printer.println(");");
	}
	void visit_intrinsic(const Intrinsic* intrinsic) override {
		if (intrinsic->get_name() == "putChar") {
			const std::size_t argument = evaluate(intrinsic->get_arguments()[0]);
			printer.println(format("  document.write(String.fromCharCode(v%).replace('\\n', '<br>'));", print_number(argument)));
			result = function_table.variable++;
			printer.println(format("  let v% = null;", print_number(result)));
		}
		else if (intrinsic->get_name() == "getChar") {
			// TODO
		}
	}
	static void codegen(const Expression* expression, const char* path) {
		FunctionTable function_table;
		Printer printer(stdout);
		printer.println("<html><head><script>");
		{
			// the main function
			printer.println("window.onload = main;");
			printer.println("function main() {");
			CodegenJS codegen(function_table, 0, printer);
			codegen.evaluate(expression);
			printer.println("}");
		}
		while (function_table.done < function_table.size()) {
			const std::size_t index = function_table.done;
			printer.print(format("function f%(obj", print_number(index)));
			for (std::size_t i = 0; i < function_table[index].arguments; ++i) {
				printer.print(format(", a%", print_number(i)));
			}
			printer.println(") {");
			CodegenJS codegen(function_table, index, printer);
			const std::size_t result = codegen.evaluate(function_table[index].function->get_expression());
			printer.println(format("  return v%;", print_number(result)));
			printer.println("}");
			++function_table.done;
		}
		printer.println("</script></head><body></body></html>");
	}
};

void codegen(const Expression* expression, const char* path) {
	expression = Pass1::run(expression);
	CodegenX86::codegen(expression, path);
}
