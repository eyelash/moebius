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
		if (id1 == ClosureType::id) {
			const ClosureType* closure_type1 = static_cast<const ClosureType*>(type1);
			const ClosureType* closure_type2 = static_cast<const ClosureType*>(type2);
			if (closure_type1->get_function() != closure_type2->get_function()) {
				return false;
			}
			return equals(closure_type1->get_environment_types(), closure_type2->get_environment_types());
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
			case ClosureType::id: return "Function";
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
		const Function* new_function = nullptr;
		FunctionTableEntry(const Function* old_function, const std::vector<const Type*>& argument_types): old_function(old_function), argument_types(argument_types) {}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::size_t recursion = -1;
		std::size_t look_up(const Function* old_function, const std::vector<const Type*>& argument_types) const {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				const FunctionTableEntry& entry = functions[index];
				if (entry.old_function == old_function && equals(entry.argument_types, argument_types)) {
					return index;
				}
			}
			return index;
		}
		void add_entry(const Function* old_function, const std::vector<const Type*>& argument_types) {
			functions.emplace_back(old_function, argument_types);
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
	Expression* expression;
	FunctionTable& function_table;
	std::size_t index;
	std::map<const Expression*, const Expression*> cache;
	Pass1(FunctionTable& function_table, std::size_t index): expression(nullptr), function_table(function_table), index(index) {}
	Expression* evaluate(const Expression* expression) {
		this->expression = nullptr;
		expression->accept(this);
		Expression* result = this->expression;
		cache[expression] = result;
		this->expression = nullptr;
		return result;
	}
public:
	void visit_number(const Number* number) override {
		expression = new Number(number->get_value());
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const Expression* left = cache[binary_expression->get_left()];
		const Expression* right = cache[binary_expression->get_right()];
		if ((left->get_type_id() == NumberType::id || left->get_type_id() == NeverType::id) && (right->get_type_id() == NumberType::id || right->get_type_id() == NeverType::id)) {
			expression = new BinaryExpression(binary_expression->get_operation(), left, right);
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	void visit_if(const If* if_) override {
		const Expression* condition = cache[if_->get_condition()];
		if (condition->get_type_id() == NumberType::id || condition->get_type_id() == NeverType::id) {
			If* new_if = new If(condition);
			for (const Expression* expression: if_->get_then_block()) {
				new_if->get_then_block()->add_expression(evaluate(expression));
			}
			for (const Expression* expression: if_->get_else_block()) {
				new_if->get_else_block()->add_expression(evaluate(expression));
			}
			const Expression* then_expression = cache[if_->get_then_expression()];
			const Expression* else_expression = cache[if_->get_else_expression()];
			new_if->set_then_expression(then_expression);
			new_if->set_else_expression(else_expression);
			if (equals(then_expression->get_type(), else_expression->get_type())) {
				new_if->set_type(then_expression->get_type());
			}
			else if (then_expression->get_type_id() == NeverType::id) {
				new_if->set_type(else_expression->get_type());
			}
			else if (else_expression->get_type_id() == NeverType::id) {
				new_if->set_type(then_expression->get_type());
			}
			else {
				error(if_, "if and else branches must return values of the same type");
			}
			expression = new_if;
		}
		else {
			error(if_, "type of condition must be a number");
		}
	}
	void visit_closure(const Closure* closure) override {
		ClosureType* type = new ClosureType(closure->get_function());
		Closure* new_closure = new Closure(nullptr, type);
		for (const Expression* expression: closure->get_environment_expressions()) {
			const Expression* new_expression = cache[expression];
			type->add_environment_type(new_expression->get_type());
			new_closure->add_environment_expression(new_expression);
		}
		expression = new_closure;
	}
	void visit_closure_access(const ClosureAccess* closure_access) override {
		const std::size_t argument_index = closure_access->get_index();
		const Expression* closure = cache[closure_access->get_closure()];
		const ClosureType* closure_type = static_cast<const ClosureType*>(closure->get_type());
		const Type* type = closure_type->get_environment_types()[argument_index];
		expression = new ClosureAccess(closure, argument_index, type);
	}
	void visit_argument(const Argument* argument) override {
		const std::size_t argument_index = argument->get_index();
		const Type* type = function_table[index].argument_types[argument_index];
		expression = new Argument(argument_index, type);
	}
	void visit_call(const Call* call) override {
		Call* new_call = new Call();
		std::vector<const Type*> argument_types;
		for (const Expression* argument: call->get_arguments()) {
			const Expression* new_argument = cache[argument];
			argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		const Expression* object = new_call->get_object();
		if (object->get_type_id() != ClosureType::id) {
			error("call to a value that is not a function");
		}
		const Function* old_function = static_cast<const ClosureType*>(object->get_type())->get_function();
		if (call->get_arguments().size() != old_function->get_arguments()) {
			error(format("call with % arguments to a function that accepts % arguments", print_number(call->get_arguments().size() - 1), print_number(old_function->get_arguments() - 1)));
		}

		const std::size_t new_index = function_table.look_up(old_function, argument_types);
		if (new_index == function_table.size()) {
			function_table.add_entry(old_function, argument_types);
			Pass1 pass1(function_table, new_index);
			Function* new_function = new Function();
			for (const Expression* expression: old_function->get_block()) {
				new_function->get_block()->add_expression(pass1.evaluate(expression));
			}
			const Expression* new_expression = pass1.cache[old_function->get_expression()];
			new_function->set_expression(new_expression);
			new_function->set_type(new_expression->get_type());
			function_table[new_index].new_function = new_function;
			if (function_table.recursion == new_index) {
				// reevaluate the expression in case of recursion
				function_table.recursion = -1;
				function_table.clear(new_index);
				new_function->get_block()->clear();
				for (const Expression* expression: old_function->get_block()) {
					new_function->get_block()->add_expression(pass1.evaluate(expression));
				}
				new_expression = pass1.cache[old_function->get_expression()];
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
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic->get_name(), intrinsic->get_type());
		for (const Expression* argument: intrinsic->get_arguments()) {
			new_intrinsic->add_argument(cache[argument]);
		}
		if (intrinsic->name_equals("putChar")) {
			// assert(new_intrinsic->get_arguments().size() == 1);
			if (new_intrinsic->get_arguments()[0]->get_type_id() != NumberType::id) {
				error("argument of putChar must be a number");
			}
		}
		expression = new_intrinsic;
	}
	void visit_bind(const Bind* bind) override {
		const Expression* left = cache[bind->get_left()];
		const Expression* right = cache[bind->get_right()];
		expression = new Bind(left, right);
	}
	static const Function* run(const Function* main_function) {
		FunctionTable function_table;
		Pass1 pass1(function_table, 0);
		Function* new_function = new Function();
		for (const Expression* expression: main_function->get_block()) {
			new_function->get_block()->add_expression(pass1.evaluate(expression));
		}
		new_function->set_expression(pass1.cache[main_function->get_expression()]);
		return new_function;
	}
};

// inlining
class Pass2 {
	struct FunctionTableEntry {
		const Function* function;
		const Function* new_function = nullptr;
		std::size_t callers = 0;
		FunctionTableEntry(const Function* function): function(function) {}
		bool should_inline() const {
			return callers == 1;
		}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::size_t look_up(const Function* function) const {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				if (functions[index].function == function) {
					return index;
				}
			}
			return index;
		}
		void add_entry(const Function* function) {
			functions.emplace_back(function);
		}
		FunctionTableEntry& operator [](std::size_t index) {
			return functions[index];
		}
		std::size_t size() const {
			return functions.size();
		}
	};
	class Analyze: public Visitor {
		FunctionTable& function_table;
	public:
		Analyze(FunctionTable& function_table): function_table(function_table) {}
		void evaluate(const Block& block) {
			for (const Expression* expression: block) {
				expression->accept(this);
			}
		}
		void visit_number(const Number* number) override {}
		void visit_binary_expression(const BinaryExpression* binary_expression) override {}
		void visit_if(const If* if_) override {
			evaluate(if_->get_then_block());
			evaluate(if_->get_else_block());
		}
		void visit_closure(const Closure* closure) override {}
		void visit_closure_access(const ClosureAccess* closure_access) override {}
		void visit_argument(const Argument* argument) override {}
		void visit_call(const Call* call) override {
			const std::size_t new_index = function_table.look_up(call->get_function());
			if (new_index == function_table.size()) {
				function_table.add_entry(call->get_function());
				evaluate(call->get_function()->get_block());
			}
			++function_table[new_index].callers;
		}
		void visit_intrinsic(const Intrinsic* intrinsic) override {}
		void visit_bind(const Bind* bind) override {}
	};
	class Replace: public Visitor {
		Expression* expression;
		Block* current_block;
		FunctionTable& function_table;
		std::size_t index;
		std::vector<Expression*> arguments;
		std::map<const Expression*, Expression*> cache;
	public:
		Replace(FunctionTable& function_table, std::size_t index): expression(nullptr), function_table(function_table), index(index) {}
		void evaluate(Block* destination_block, const Block& source_block) {
			Block* previous_block = current_block;
			current_block = destination_block;
			for (const Expression* expression: source_block) {
				this->expression = nullptr;
				expression->accept(this);
				if (this->expression) {
					cache[expression] = this->expression;
					destination_block->add_expression(this->expression);
					this->expression = nullptr;
				}
			}
			destination_block->set_result(cache[source_block.get_result()]);
			current_block = previous_block;
		}
		void visit_number(const Number* number) override {
			expression = new Number(number->get_value());
		}
		void visit_binary_expression(const BinaryExpression* binary_expression) override {
			const Expression* left = cache[binary_expression->get_left()];
			const Expression* right = cache[binary_expression->get_right()];
			expression = new BinaryExpression(binary_expression->get_operation(), left, right);
		}
		void visit_if(const If* if_) override {
			const Expression* condition = cache[if_->get_condition()];
			If* new_if = new If(condition, if_->get_type());
			evaluate(new_if->get_then_block(), if_->get_then_block());
			evaluate(new_if->get_else_block(), if_->get_else_block());
			expression = new_if;
		}
		void visit_closure(const Closure* closure) override {
			Closure* new_closure = new Closure(nullptr, closure->get_type());
			for (const Expression* expression: closure->get_environment_expressions()) {
				new_closure->add_environment_expression(cache[expression]);
			}
			expression = new_closure;
		}
		void visit_closure_access(const ClosureAccess* closure_access) override {
			const Expression* closure = cache[closure_access->get_closure()];
			expression = new ClosureAccess(closure, closure_access->get_index(), closure_access->get_type());
		}
		void visit_argument(const Argument* argument) override {
			if (function_table[index].should_inline()) {
				cache[argument] = arguments[argument->get_index()];
			}
			else {
				expression = new Argument(argument->get_index(), argument->get_type());
			}
		}
		void visit_call(const Call* call) override {
			const std::size_t new_index = function_table.look_up(call->get_function());
			if (function_table[new_index].should_inline()) {
				Replace replace(function_table, new_index);
				for (const Expression* argument: call->get_arguments()) {
					Expression* new_argument = cache[argument];
					replace.arguments.push_back(new_argument);
				}
				replace.evaluate(current_block, call->get_function()->get_block());
				cache[call] = replace.cache[call->get_function()->get_expression()];
				expression = nullptr;
			}
			else {
				Call* new_call = new Call();
				for (const Expression* argument: call->get_arguments()) {
					const Expression* new_argument = cache[argument];
					new_call->add_argument(new_argument);
				}
				if (function_table[new_index].new_function == nullptr) {
					Function* new_function = new Function();
					new_function->set_type(call->get_function()->get_type());
					function_table[new_index].new_function = new_function;
					Replace replace(function_table, new_index);
					replace.evaluate(new_function->get_block(), call->get_function()->get_block());
				}
				new_call->set_type(function_table[new_index].new_function->get_type());
				new_call->set_function(function_table[new_index].new_function);
				expression = new_call;
			}
		}
		void visit_intrinsic(const Intrinsic* intrinsic) override {
			Intrinsic* new_intrinsic = new Intrinsic(intrinsic->get_name(), intrinsic->get_type());
			for (const Expression* argument: intrinsic->get_arguments()) {
				new_intrinsic->add_argument(cache[argument]);
			}
			expression = new_intrinsic;
		}
		void visit_bind(const Bind* bind) override {
			const Expression* left = cache[bind->get_left()];
			const Expression* right = cache[bind->get_right()];
			expression = new Bind(left, right);
		}
	};
public:
	Pass2() = delete;
	static const Function* run(const Function* main_function) {
		FunctionTable function_table;
		Analyze analyze(function_table);
		analyze.evaluate(main_function->get_block());
		Replace replace(function_table, 0);
		Function* new_function = new Function(main_function->get_type());
		replace.evaluate(new_function->get_block(), main_function->get_block());
		return new_function;
	}
};

class CodegenX86: public Visitor {
	using A = Assembler;
	using Jump = typename A::Jump;
	static std::uint32_t get_type_size(const Type* type) {
		switch (type->get_id()) {
			case NumberType::id:
				return 4;
			case ClosureType::id: {
				int size = 0;
				for (const Type* type: static_cast<const ClosureType*>(type)->get_environment_types()) {
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
		std::uint32_t input_size;
		std::uint32_t output_size;
		std::size_t position;
		FunctionTableEntry(const Function* function, const std::vector<const Type*>& argument_types): function(function), argument_types(argument_types) {
			input_size = 0;
			for (const Type* type: argument_types) {
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
			return nullptr;
		}
	};
	class FunctionTable {
		std::vector<FunctionTableEntry> functions;
	public:
		std::vector<DeferredCall> deferred_calls;
		std::size_t done = 0;
		std::size_t look_up(const Function* function, const std::vector<const Type*>& argument_types) {
			std::size_t index;
			for (index = 0; index < functions.size(); ++index) {
				if (functions[index].function == function) {
					return index;
				}
			}
			functions.emplace_back(function, argument_types);
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
		evaluate(binary_expression->get_left());
		evaluate(binary_expression->get_right());
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
	void visit_closure(const Closure* closure) override {
		for (const Expression* expression: closure->get_environment_expressions()) {
			evaluate(expression);
		}
	}
	void visit_closure_access(const ClosureAccess* closure_access) override {
		evaluate(closure_access->get_closure());
		// assert(closure_access->get_closure()->get_type_id() == ClosureType::id);
		const std::vector<const Type*>& types = static_cast<const ClosureType*>(closure_access->get_closure()->get_type())->get_environment_types();
		std::uint32_t before = 0, size = 0, after = 0;
		for (std::size_t i = 0; i < types.size(); ++i) {
			if (i < closure_access->get_index()) before += get_type_size(types[i]);
			else if (i == closure_access->get_index()) size += get_type_size(types[i]);
			else if (i > closure_access->get_index()) after += get_type_size(types[i]);
		}
		if (after > 0) {
			assembler.ADD(ESP, after);
		}
		if (before > 0) {
			for (std::uint32_t i = 0; i < size; i += 4) {
				assembler.MOV(EAX, PTR(ESP, i));
				assembler.MOV(PTR(ESP, before + i), EAX);
			}
			assembler.ADD(ESP, before);
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
		const std::size_t new_index = function_table.look_up(call->get_function(), argument_types);
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
		if (intrinsic->name_equals("putChar")) {
			evaluate(intrinsic->get_arguments()[0]);
			assembler.comment("putChar");
			assembler.MOV(EAX, 0x04);
			assembler.MOV(EBX, 1); // stdout
			assembler.MOV(ECX, ESP);
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.POP(EAX);
		}
		else if (intrinsic->name_equals("getChar")) {
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
	void visit_bind(const Bind* bind) override {
		evaluate(bind->get_left());
		evaluate(bind->get_right());
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
			function_table.done += 1;
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
	std::size_t variable = 1;
	std::map<const Expression*, std::size_t> cache;
	CodegenJS(FunctionTable& function_table, std::size_t index, Printer& printer): function_table(function_table), index(index), printer(printer) {}
	void evaluate(const Block& block) {
		for (const Expression* expression: block) {
			result = 0;
			expression->accept(this);
			cache[expression] = result;
		}
	}
public:
	void visit_number(const Number* number) override {
		result = variable++;
		printer.println(format("  const v% = %;", print_number(result), print_number(number->get_value())));
	}
	void visit_binary_expression(const BinaryExpression* binary_expression) override {
		const std::size_t left = cache[binary_expression->get_left()];
		const std::size_t right = cache[binary_expression->get_right()];
		result = variable++;
		printer.println(format("  const v% = (v% % v%) | 0;", print_number(result), print_number(left), print_operator(binary_expression->get_operation()), print_number(right)));
	}
	void visit_if(const If* if_) override {
		const std::size_t condition = cache[if_->get_condition()];
		const std::size_t result = variable++;
		printer.println(format("  let v%;", print_number(result)));
		printer.println(format("  if (v%) {", print_number(condition)));
		evaluate(if_->get_then_block());
		const std::size_t inner_then = cache[if_->get_then_expression()];
		printer.println(format("  v% = v%;", print_number(result), print_number(inner_then)));
		printer.println("  } else {");
		evaluate(if_->get_else_block());
		const std::size_t inner_else = cache[if_->get_else_expression()];
		printer.println(format("  v% = v%;", print_number(result), print_number(inner_else)));
		printer.println("  }");
		this->result = result;
	}
	void visit_closure(const Closure* closure) override {
		std::vector<std::size_t> elements;
		for (const Expression* element: closure->get_environment_expressions()) {
			elements.push_back(cache[element]);
		}
		result = variable++;
		printer.print(format("  const v% = [", print_number(result)));
		for (const std::size_t element: elements) {
			printer.print(format("v%,", print_number(element)));
		}
		printer.println("];");
	}
	void visit_closure_access(const ClosureAccess* closure_access) override {
		const std::size_t closure = cache[closure_access->get_closure()];
		result = variable++;
		printer.println(format("  const v% = v%[%];", print_number(result), print_number(closure), print_number(closure_access->get_index())));
	}
	void visit_argument(const Argument* argument) override {
		result = argument->get_index();
	}
	void visit_call(const Call* call) override {
		std::vector<std::size_t> arguments;
		for (const Expression* argument: call->get_arguments()) {
			arguments.push_back(cache[argument]);
		}
		const std::size_t new_index = function_table.look_up(call->get_function(), arguments.size());
		result = variable++;
		printer.print(format("  const v% = f%(", print_number(result), print_number(new_index)));
		for (const std::size_t argument: arguments) {
			printer.print(format("v%,", print_number(argument)));
		}
		printer.println(");");
	}
	void visit_intrinsic(const Intrinsic* intrinsic) override {
		if (intrinsic->name_equals("putChar")) {
			const std::size_t argument = cache[intrinsic->get_arguments()[0]];
			printer.println(format("  const s = String.fromCharCode(v%);", print_number(argument)));
			printer.println("  document.body.appendChild(s === '\\n' ? document.createElement('br') : document.createTextNode(s));");
			result = variable++;
			printer.println(format("  const v% = null;", print_number(result)));
		}
		else if (intrinsic->name_equals("getChar")) {
			// TODO
		}
	}
	void visit_bind(const Bind* bind) override {}
	static void codegen(const Function* main_function, const char* path) {
		FunctionTable function_table;
		Printer printer(stdout);
		printer.println("<!DOCTYPE html><html><head><script>");
		{
			// the main function
			printer.println("window.addEventListener('load', main);");
			printer.println("function main() {");
			CodegenJS codegen(function_table, 0, printer);
			codegen.evaluate(main_function->get_block());
			printer.println("}");
		}
		while (function_table.done < function_table.size()) {
			const std::size_t index = function_table.done;
			printer.print(format("function f%(", print_number(index)));
			for (std::size_t i = 0; i < function_table[index].arguments; ++i) {
				printer.print(format("v%,", print_number(i)));
			}
			printer.println(") {");
			CodegenJS codegen(function_table, index, printer);
			codegen.variable = function_table[index].arguments;
			codegen.evaluate(function_table[index].function->get_block());
			const std::size_t result = codegen.cache[function_table[index].function->get_expression()];
			printer.println(format("  return v%;", print_number(result)));
			printer.println("}");
			function_table.done += 1;
		}
		printer.println("</script></head><body></body></html>");
	}
};

void codegen(const Function* main_function, const char* path) {
	main_function = Pass1::run(main_function);
	main_function = Pass2::run(main_function);
	CodegenJS::codegen(main_function, path);
}
