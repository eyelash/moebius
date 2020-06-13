#pragma once

#include "ast.hpp"
#include "assembler.hpp"
#include <cstdlib>

// type checking and monomorphization
class Pass1: public Visitor<Expression*> {
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
	template <class T> [[noreturn]] void error(const Expression& expression, const T& t) {
		Printer printer(stderr);
		expression.get_position().print_error(printer, t);
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
	FunctionTable& function_table;
	std::size_t index;
	std::map<const Expression*, const Expression*> cache;
	Pass1(FunctionTable& function_table, std::size_t index): function_table(function_table), index(index) {}
	void evaluate(Block* destination_block, const Block& source_block) {
		for (const Expression* expression: source_block) {
			Expression* result = visit(*this, expression);
			cache[expression] = result;
			destination_block->add_expression(result);
		}
	}
public:
	Expression* visit_number(const Number& number) override {
		return new Number(number.get_value());
	}
	Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
		const Expression* left = cache[binary_expression.get_left()];
		const Expression* right = cache[binary_expression.get_right()];
		if ((left->get_type_id() == NumberType::id || left->get_type_id() == NeverType::id) && (right->get_type_id() == NumberType::id || right->get_type_id() == NeverType::id)) {
			return new BinaryExpression(binary_expression.get_operation(), left, right);
		}
		else {
			error(binary_expression, format("binary expression of types % and %", print_type(left->get_type()), print_type(right->get_type())));
		}
	}
	Expression* visit_if(const If& if_) override {
		const Expression* condition = cache[if_.get_condition()];
		if (condition->get_type_id() == NumberType::id || condition->get_type_id() == NeverType::id) {
			If* new_if = new If(condition);
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			const Expression* then_expression = cache[if_.get_then_expression()];
			const Expression* else_expression = cache[if_.get_else_expression()];
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
			return new_if;
		}
		else {
			error(if_, "type of condition must be a number");
		}
	}
	Expression* visit_closure(const Closure& closure) override {
		ClosureType* type = new ClosureType(closure.get_function());
		Closure* new_closure = new Closure(nullptr, type);
		for (const Expression* expression: closure.get_environment_expressions()) {
			const Expression* new_expression = cache[expression];
			type->add_environment_type(new_expression->get_type());
			new_closure->add_environment_expression(new_expression);
		}
		return new_closure;
	}
	Expression* visit_closure_access(const ClosureAccess& closure_access) override {
		const std::size_t argument_index = closure_access.get_index();
		const Expression* closure = cache[closure_access.get_closure()];
		const ClosureType* closure_type = static_cast<const ClosureType*>(closure->get_type());
		const Type* type = closure_type->get_environment_types()[argument_index];
		return new ClosureAccess(closure, argument_index, type);
	}
	Expression* visit_argument(const Argument& argument) override {
		const std::size_t argument_index = argument.get_index();
		const Type* type = function_table[index].argument_types[argument_index];
		return new Argument(argument_index, type);
	}
	Expression* visit_call(const Call& call) override {
		Call* new_call = new Call();
		std::vector<const Type*> argument_types;
		for (const Expression* argument: call.get_arguments()) {
			const Expression* new_argument = cache[argument];
			argument_types.push_back(new_argument->get_type());
			new_call->add_argument(new_argument);
		}
		const Expression* object = new_call->get_object();
		if (object->get_type_id() != ClosureType::id) {
			error("call to a value that is not a function");
		}
		const Function* old_function = static_cast<const ClosureType*>(object->get_type())->get_function();
		if (call.get_arguments().size() != old_function->get_arguments()) {
			error(format("call with % arguments to a function that accepts % arguments", print_number(call.get_arguments().size() - 1), print_number(old_function->get_arguments() - 1)));
		}

		const std::size_t new_index = function_table.look_up(old_function, argument_types);
		if (new_index == function_table.size()) {
			function_table.add_entry(old_function, argument_types);
			Pass1 pass1(function_table, new_index);
			Function* new_function = new Function();
			pass1.evaluate(new_function->get_block(), old_function->get_block());
			const Expression* new_expression = pass1.cache[old_function->get_expression()];
			new_function->set_expression(new_expression);
			new_function->set_type(new_expression->get_type());
			function_table[new_index].new_function = new_function;
			if (function_table.recursion == new_index) {
				// reevaluate the expression in case of recursion
				function_table.recursion = -1;
				function_table.clear(new_index);
				new_function->get_block()->clear();
				pass1.evaluate(new_function->get_block(), old_function->get_block());
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
		return new_call;
	}
	Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
		Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name(), intrinsic.get_type());
		for (const Expression* argument: intrinsic.get_arguments()) {
			new_intrinsic->add_argument(cache[argument]);
		}
		if (intrinsic.name_equals("putChar")) {
			// assert(new_intrinsic->get_arguments().size() == 1);
			if (new_intrinsic->get_arguments()[0]->get_type_id() != NumberType::id) {
				error("argument of putChar must be a number");
			}
		}
		return new_intrinsic;
	}
	Expression* visit_bind(const Bind& bind) override {
		const Expression* left = cache[bind.get_left()];
		const Expression* right = cache[bind.get_right()];
		return new Bind(left, right);
	}
	static const Function* run(const Function* main_function) {
		FunctionTable function_table;
		Pass1 pass1(function_table, 0);
		Function* new_function = new Function();
		pass1.evaluate(new_function->get_block(), main_function->get_block());
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
	class Analyze: public Visitor<void> {
		FunctionTable& function_table;
	public:
		Analyze(FunctionTable& function_table): function_table(function_table) {}
		void evaluate(const Block& block) {
			for (const Expression* expression: block) {
				visit(*this, expression);
			}
		}
		void visit_number(const Number& number) override {}
		void visit_binary_expression(const BinaryExpression& binary_expression) override {}
		void visit_if(const If& if_) override {
			evaluate(if_.get_then_block());
			evaluate(if_.get_else_block());
		}
		void visit_closure(const Closure& closure) override {}
		void visit_closure_access(const ClosureAccess& closure_access) override {}
		void visit_argument(const Argument& argument) override {}
		void visit_call(const Call& call) override {
			const std::size_t new_index = function_table.look_up(call.get_function());
			if (new_index == function_table.size()) {
				function_table.add_entry(call.get_function());
				evaluate(call.get_function()->get_block());
			}
			++function_table[new_index].callers;
		}
		void visit_intrinsic(const Intrinsic& intrinsic) override {}
		void visit_bind(const Bind& bind) override {}
	};
	class Replace: public Visitor<Expression*> {
		Block* current_block;
		FunctionTable& function_table;
		std::size_t index;
		std::vector<Expression*> arguments;
		std::map<const Expression*, Expression*> cache;
	public:
		Replace(FunctionTable& function_table, std::size_t index): function_table(function_table), index(index) {}
		void evaluate(Block* destination_block, const Block& source_block) {
			Block* previous_block = current_block;
			current_block = destination_block;
			for (const Expression* expression: source_block) {
				Expression* result = visit(*this, expression);
				if (result) {
					cache[expression] = result;
					destination_block->add_expression(result);
				}
			}
			destination_block->set_result(cache[source_block.get_result()]);
			current_block = previous_block;
		}
		Expression* visit_number(const Number& number) override {
			return new Number(number.get_value());
		}
		Expression* visit_binary_expression(const BinaryExpression& binary_expression) override {
			const Expression* left = cache[binary_expression.get_left()];
			const Expression* right = cache[binary_expression.get_right()];
			return new BinaryExpression(binary_expression.get_operation(), left, right);
		}
		Expression* visit_if(const If& if_) override {
			const Expression* condition = cache[if_.get_condition()];
			If* new_if = new If(condition, if_.get_type());
			evaluate(new_if->get_then_block(), if_.get_then_block());
			evaluate(new_if->get_else_block(), if_.get_else_block());
			return new_if;
		}
		Expression* visit_closure(const Closure& closure) override {
			Closure* new_closure = new Closure(nullptr, closure.get_type());
			for (const Expression* expression: closure.get_environment_expressions()) {
				new_closure->add_environment_expression(cache[expression]);
			}
			return new_closure;
		}
		Expression* visit_closure_access(const ClosureAccess& closure_access) override {
			const Expression* closure = cache[closure_access.get_closure()];
			return new ClosureAccess(closure, closure_access.get_index(), closure_access.get_type());
		}
		Expression* visit_argument(const Argument& argument) override {
			if (function_table[index].should_inline()) {
				cache[&argument] = arguments[argument.get_index()];
				return nullptr;
			}
			else {
				return new Argument(argument.get_index(), argument.get_type());
			}
		}
		Expression* visit_call(const Call& call) override {
			const std::size_t new_index = function_table.look_up(call.get_function());
			if (function_table[new_index].should_inline()) {
				Replace replace(function_table, new_index);
				for (const Expression* argument: call.get_arguments()) {
					Expression* new_argument = cache[argument];
					replace.arguments.push_back(new_argument);
				}
				replace.evaluate(current_block, call.get_function()->get_block());
				cache[&call] = replace.cache[call.get_function()->get_expression()];
				return nullptr;
			}
			else {
				Call* new_call = new Call();
				for (const Expression* argument: call.get_arguments()) {
					const Expression* new_argument = cache[argument];
					new_call->add_argument(new_argument);
				}
				if (function_table[new_index].new_function == nullptr) {
					Function* new_function = new Function(call.get_function()->get_type());
					function_table[new_index].new_function = new_function;
					Replace replace(function_table, new_index);
					replace.evaluate(new_function->get_block(), call.get_function()->get_block());
				}
				new_call->set_type(function_table[new_index].new_function->get_type());
				new_call->set_function(function_table[new_index].new_function);
				return new_call;
			}
		}
		Expression* visit_intrinsic(const Intrinsic& intrinsic) override {
			Intrinsic* new_intrinsic = new Intrinsic(intrinsic.get_name(), intrinsic.get_type());
			for (const Expression* argument: intrinsic.get_arguments()) {
				new_intrinsic->add_argument(cache[argument]);
			}
			return new_intrinsic;
		}
		Expression* visit_bind(const Bind& bind) override {
			const Expression* left = cache[bind.get_left()];
			const Expression* right = cache[bind.get_right()];
			return new Bind(left, right);
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

class CodegenX86: public Visitor<std::uint32_t> {
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
	std::uint32_t variable = 0;
	std::map<const Expression*,std::uint32_t> cache;
	CodegenX86(FunctionTable& function_table, std::size_t index, A& assembler): function_table(function_table), index(index), assembler(assembler) {}
	void evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
	}
	std::uint32_t allocate(std::uint32_t size) {
		variable -= size;
		return variable;
	}
	void memcopy(std::uint32_t destination, std::uint32_t source, std::uint32_t size) {
		for (std::uint32_t i = 0; i < size; i += 4) {
			assembler.MOV(EAX, PTR(EBP, source + i));
			assembler.MOV(PTR(EBP, destination + i), EAX);
		}
	}
public:
	std::uint32_t visit_number(const Number& number) override {
		const std::uint32_t result = allocate(4);
		assembler.MOV(EAX, number.get_value());
		assembler.MOV(PTR(EBP, result), EAX);
		return result;
	}
	std::uint32_t visit_binary_expression(const BinaryExpression& binary_expression) override {
		const std::uint32_t left = cache[binary_expression.get_left()];
		const std::uint32_t right = cache[binary_expression.get_right()];
		assembler.MOV(EAX, PTR(EBP, left));
		assembler.MOV(EBX, PTR(EBP, right));
		const std::uint32_t result = allocate(4);
		switch (binary_expression.get_operation()) {
			case BinaryOperation::ADD:
				assembler.ADD(EAX, EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::SUB:
				assembler.SUB(EAX, EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::MUL:
				assembler.IMUL(EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::DIV:
				assembler.CDQ();
				assembler.IDIV(EBX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::REM:
				assembler.CDQ();
				assembler.IDIV(EBX);
				assembler.MOV(PTR(EBP, result), EDX);
				break;
			case BinaryOperation::EQ:
				assembler.CMP(EAX, EBX);
				assembler.SETE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::NE:
				assembler.CMP(EAX, EBX);
				assembler.SETNE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::LT:
				assembler.CMP(EAX, EBX);
				assembler.SETL(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::LE:
				assembler.CMP(EAX, EBX);
				assembler.SETLE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::GT:
				assembler.CMP(EAX, EBX);
				assembler.SETG(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
			case BinaryOperation::GE:
				assembler.CMP(EAX, EBX);
				assembler.SETGE(EAX);
				assembler.MOVZX(EAX, EAX);
				assembler.MOV(PTR(EBP, result), EAX);
				break;
		}
		return result;
	}
	std::uint32_t visit_if(const If& if_) override {
		const std::uint32_t condition = cache[if_.get_condition()];
		assembler.MOV(EAX, PTR(EBP, condition));
		assembler.CMP(EAX, 0);
		const std::uint32_t size = get_type_size(if_.get_type());
		const std::uint32_t if_result = allocate(size);
		const Jump jump_else = assembler.JE();
		assembler.comment("if");
		evaluate(if_.get_then_block());
		const std::uint32_t then_result = cache[if_.get_then_expression()];
		memcopy(if_result, then_result, size);
		const Jump jump_end = assembler.JMP();
		assembler.comment("else");
		jump_else.set_target(assembler.get_position());
		evaluate(if_.get_else_block());
		const std::uint32_t else_result = cache[if_.get_else_expression()];
		memcopy(if_result, else_result, size);
		assembler.comment("end");
		jump_end.set_target(assembler.get_position());
		return if_result;
	}
	std::uint32_t visit_closure(const Closure& closure) override {
		const std::uint32_t size = get_type_size(closure.get_type());
		const std::vector<const Expression*>& environment = closure.get_environment_expressions();
		if (environment.size() == 1) {
			return cache[environment[0]];
		}
		else if (environment.size() > 1) {
			std::uint32_t destination = allocate(size);
			const std::uint32_t result = destination;
			for (const Expression* expression: environment) {
				const std::uint32_t element_size = get_type_size(expression->get_type());
				memcopy(destination, cache[expression], element_size);
				destination += element_size;
			}
			return result;
		}
	}
	std::uint32_t visit_closure_access(const ClosureAccess& closure_access) override {
		const std::uint32_t closure = cache[closure_access.get_closure()];
		// assert(closure_access.get_closure()->get_type_id() == ClosureType::id);
		const std::vector<const Type*>& types = static_cast<const ClosureType*>(closure_access.get_closure()->get_type())->get_environment_types();
		std::uint32_t before = 0;
		for (std::size_t i = 0; i < closure_access.get_index(); ++i) {
			before += get_type_size(types[i]);
		}
		return closure + before;
	}
	std::uint32_t visit_argument(const Argument& argument) override {
		std::uint32_t location;
		function_table[index].look_up(argument.get_index(), location);
		return 8 + location;
	}
	std::uint32_t visit_call(const Call& call) override {
		std::vector<const Type*> argument_types;
		for (const Expression* argument: call.get_arguments()) {
			argument_types.push_back(argument->get_type());
		}
		const std::size_t new_index = function_table.look_up(call.get_function(), argument_types);
		const std::uint32_t input_size = function_table[new_index].input_size;
		const std::uint32_t output_size = function_table[new_index].output_size;
		std::uint32_t destination = variable;
		for (const Expression* argument: call.get_arguments()) {
			const std::uint32_t argument_size = get_type_size(argument->get_type());
			destination -= argument_size;
			memcopy(destination, cache[argument], argument_size);
		}
		assembler.LEA(ESP, PTR(EBP, destination));
		if (output_size > input_size) {
			// negative in order to grow the stack
			assembler.ADD(ESP, input_size - output_size);
		}
		Jump jump = assembler.CALL();
		function_table.deferred_calls.emplace_back(jump, new_index);
		if (output_size < input_size) {
			assembler.ADD(ESP, input_size - output_size);
		}
		variable -= output_size;
		return variable;
	}
	std::uint32_t visit_intrinsic(const Intrinsic& intrinsic) override {
		if (intrinsic.name_equals("putChar")) {
			const std::uint32_t argument = cache[intrinsic.get_arguments()[0]];
			assembler.comment("putChar");
			assembler.MOV(EAX, 0x04);
			assembler.MOV(EBX, 1); // stdout
			assembler.LEA(ECX, PTR(EBP, argument));
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			assembler.POP(EAX);
		}
		else if (intrinsic.name_equals("getChar")) {
			const std::uint32_t result = allocate(4);
			assembler.comment("getChar");
			assembler.MOV(EAX, 0x03);
			assembler.MOV(EBX, 0); // stdin
			assembler.LEA(ECX, PTR(EBP, result));
			assembler.MOV(PTR(ECX), EBX);
			assembler.MOV(EDX, 1);
			assembler.INT(0x80);
			return result;
		}
	}
	std::uint32_t visit_bind(const Bind& bind) override {}
	static void codegen(const Function* main_function, const char* path) {
		FunctionTable function_table;
		A assembler;
		{
			// the main function
			CodegenX86 codegen(function_table, 0, assembler);
			assembler.MOV(EBP, ESP);
			codegen.evaluate(main_function->get_block());
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
			codegen.evaluate(function_table[index].function->get_block());
			const std::uint32_t output_location = codegen.cache[function_table[index].function->get_expression()];
			assembler.comment("--");
			assembler.MOV(ESP, EBP);
			assembler.ADD(ESP, output_location);
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
	FunctionTable& function_table;
	const std::size_t index;
	Printer& printer;
	std::size_t variable = 1;
	std::map<const Expression*, std::size_t> cache;
	CodegenJS(FunctionTable& function_table, std::size_t index, Printer& printer): function_table(function_table), index(index), printer(printer) {}
	void evaluate(const Block& block) {
		for (const Expression* expression: block) {
			cache[expression] = visit(*this, expression);
		}
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
		evaluate(if_.get_then_block());
		const std::size_t inner_then = cache[if_.get_then_expression()];
		printer.println(format("  v% = v%;", print_number(result), print_number(inner_then)));
		printer.println("  } else {");
		evaluate(if_.get_else_block());
		const std::size_t inner_else = cache[if_.get_else_expression()];
		printer.println(format("  v% = v%;", print_number(result), print_number(inner_else)));
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
		const std::size_t new_index = function_table.look_up(call.get_function(), arguments.size());
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
	CodegenX86::codegen(main_function, path);
}
