#pragma once

#include "printer.hpp"
#include "ast.hpp"
#include <map>
#include <cstdlib>

struct BinaryOperator {
	const char* string;
	using Create = Expression* (*)(const Expression* left, const Expression* right);
	Create create;
	constexpr BinaryOperator(const char* string, Create create): string(string), create(create) {}
};

using OperatorLevel = std::initializer_list<BinaryOperator>;

constexpr std::initializer_list<OperatorLevel> operators = {
	{
		BinaryOperator(">>", Bind::create)
	},
	{
		BinaryOperator("==", BinaryExpression::create<BinaryOperation::EQ>),
		BinaryOperator("!=", BinaryExpression::create<BinaryOperation::NE>)
	},
	{
		BinaryOperator("<", BinaryExpression::create<BinaryOperation::LT>),
		BinaryOperator("<=", BinaryExpression::create<BinaryOperation::LE>),
		BinaryOperator(">", BinaryExpression::create<BinaryOperation::GT>),
		BinaryOperator(">=", BinaryExpression::create<BinaryOperation::GE>)
	},
	{
		BinaryOperator("+", BinaryExpression::create<BinaryOperation::ADD>),
		BinaryOperator("-", BinaryExpression::create<BinaryOperation::SUB>)
	},
	{
		BinaryOperator("*", BinaryExpression::create<BinaryOperation::MUL>),
		BinaryOperator("/", BinaryExpression::create<BinaryOperation::DIV>),
		BinaryOperator("%", BinaryExpression::create<BinaryOperation::REM>)
	}
};

struct UnaryOperator {
	const char* string;
	using Create = Expression* (*)(const Expression* expression);
	Create create;
	constexpr UnaryOperator(const char* string, Create create): string(string), create(create) {}
};

constexpr std::initializer_list<UnaryOperator> unary_operators = {};

constexpr const char* intrinsics[] = {
	"putChar",
	"putStr",
	"getChar",
	"arrayGet",
	"arrayLength",
	"arraySplice",
	"stringPush",
	"stringIterator",
	"stringIteratorGetNext",
	"reference",
	"typeOf",
	"arrayType",
	"tupleType",
	"referenceType",
	"error",
	"import"
};

class Scope {
	Scope*& current_scope;
	Scope* parent;
	std::map<StringView, const Expression*> variables;
	Closure* closure;
	const Expression* self = nullptr;
	Block* block;
public:
	Scope(Scope*& current_scope, Closure* closure, Block* block): current_scope(current_scope), closure(closure), block(block) {
		parent = current_scope;
		current_scope = this;
	}
	Scope(Scope*& current_scope, Block* block): Scope(current_scope, nullptr, block) {}
	Scope(Scope*& current_scope): Scope(current_scope, nullptr, nullptr) {}
	~Scope() {
		current_scope = parent;
	}
	void set_self(const Expression* self) {
		this->self = self;
	}
	void add_variable(const StringView& name, const Expression* value) {
		variables[name] = value;
	}
	const Expression* look_up(const StringView& name) {
		auto iterator = variables.find(name);
		if (iterator != variables.end()) {
			return iterator->second;
		}
		if (closure) {
			if (parent) {
				if (const Expression* expression = parent->look_up(name)) {
					const std::size_t index = closure->add_environment_expression(expression);
					const Expression* argument = create<ClosureAccess>(self, index);
					add_variable(name, argument);
					return argument;
				}
			}
		}
		else if (parent) {
			return parent->look_up(name);
		}
		return nullptr;
	}
	void add_expression(Expression* expression) {
		if (block) {
			block->add_expression(expression);
		}
		else {
			parent->add_expression(expression);
		}
	}
	template <class T, class... A> T* create(A&&... arguments) {
		T* expression = new T(std::forward<A>(arguments)...);
		add_expression(expression);
		return expression;
	}
};

class Cursor {
	const SourceFile* file;
	const char* position;
public:
	Cursor(const SourceFile* file): file(file), position(file->begin()) {}
	constexpr Cursor(const SourceFile* file, const char* position): file(file), position(position) {}
	operator bool() const {
		return position < file->end();
	}
	constexpr bool operator <(const Cursor& rhs) const {
		return position < rhs.position;
	}
	constexpr char operator *() const {
		return *position;
	}
	Cursor& operator ++() {
		++position;
		return *this;
	}
	constexpr StringView operator -(const Cursor& start) const {
		return StringView(start.position, position - start.position);
	}
	const char* get_path() const {
		return file->get_path();
	}
	std::size_t get_position() const {
		return position - file->begin();
	}
};

template <class F> class CharParser {
	F f;
public:
	constexpr CharParser(F f): f(f) {}
	template <class C> bool parse(C& cursor) {
		if (cursor && f(*cursor)) {
			++cursor;
			return true;
		}
		return false;
	}
};

class StringParser {
	StringView s;
public:
	constexpr StringParser(const StringView& s): s(s) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		for (char c: s) {
			if (!(copy && *copy == c)) {
				return false;
			}
			++copy;
		}
		cursor = copy;
		return true;
	}
};

template <class P0, class P1> class SequenceParser {
	P0 p0;
	P1 p1;
public:
	constexpr SequenceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		if (!p0.parse(copy)) {
			return false;
		}
		if (!p1.parse(copy)) {
			return false;
		}
		cursor = copy;
		return true;
	}
};

template <class P0, class P1> class ChoiceParser {
	P0 p0;
	P1 p1;
public:
	constexpr ChoiceParser(P0 p0, P1 p1): p0(p0), p1(p1) {}
	template <class C> bool parse(C& cursor) {
		if (p0.parse(cursor)) {
			return true;
		}
		if (p1.parse(cursor)) {
			return true;
		}
		return false;
	}
};

template <class P> class RepeatParser {
	P p;
public:
	constexpr RepeatParser(P p): p(p) {}
	template <class C> bool parse(C& cursor) {
		while (p.parse(cursor)) {}
		return true;
	}
};

template <class P> class NotParser {
	P p;
public:
	constexpr NotParser(P p): p(p) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		if (p.parse(copy)) {
			return false;
		}
		return true;
	}
};

template <class P> class PeekParser {
	P p;
public:
	constexpr PeekParser(P p): p(p) {}
	template <class C> bool parse(C& cursor) {
		C copy = cursor;
		if (p.parse(copy)) {
			return true;
		}
		return false;
	}
};

template <class P, class = bool> struct is_parser: std::false_type {};
template <class P> struct is_parser<P, decltype(std::declval<P>().parse(std::declval<Cursor&>()))>: std::true_type {};

template <class F, class = bool> struct is_char_class: std::false_type {};
template <class F> struct is_char_class<F, decltype(std::declval<F>()(std::declval<char>()))>: std::true_type {};

class Parser {
public:
	Cursor cursor;
	static constexpr auto get_parser(char c) {
		return CharParser([c](char c2) {
			return c == c2;
		});
	}
	static constexpr StringParser get_parser(const StringView& s) {
		return StringParser(s);
	}
	static constexpr StringParser get_parser(const char* s) {
		return StringParser(s);
	}
	template <class P> static constexpr std::enable_if_t<is_parser<P>::value, P> get_parser(P p) {
		return p;
	}
	template <class F> static constexpr std::enable_if_t<is_char_class<F>::value, CharParser<F>> get_parser(F f) {
		return CharParser(f);
	}
	static constexpr auto range(char first, char last) {
		return CharParser([first, last](char c) {
			return c >= first && c <= last;
		});
	}
	template <class P0, class P1> static constexpr auto sequence(P0 p0, P1 p1) {
		return SequenceParser(get_parser(p0), get_parser(p1));
	}
	template <class P0, class P1, class P2, class... P> static constexpr auto sequence(P0 p0, P1 p1, P2 p2, P... p) {
		return sequence(sequence(p0, p1), p2, p...);
	}
	template <class P0, class P1> static constexpr auto choice(P0 p0, P1 p1) {
		return ChoiceParser(get_parser(p0), get_parser(p1));
	}
	template <class P0, class P1, class P2, class... P> static constexpr auto choice(P0 p0, P1 p1, P2 p2, P... p) {
		return choice(choice(p0, p1), p2, p...);
	}
	template <class P> static constexpr auto zero_or_more(P p) {
		return RepeatParser(get_parser(p));
	}
	template <class P> static constexpr auto one_or_more(P p) {
		return sequence(p, zero_or_more(p));
	}
	template <class P> static constexpr auto not_(P p) {
		return NotParser(get_parser(p));
	}
	template <class P> static constexpr auto peek(P p) {
		return PeekParser(get_parser(p));
	}
	static constexpr bool any_char(char c) {
		return true;
	}
	static constexpr bool white_space(char c) {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r';
	}
	static constexpr bool numeric(char c) {
		return c >= '0' && c <= '9';
	}
	static constexpr bool alphabetic(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}
	static constexpr bool alphanumeric(char c) {
		return alphabetic(c) || numeric(c);
	}
	static constexpr bool operator_char(char c) {
		return StringView("+-*/%=<>!&|~^?:").contains(c);
	}
	template <class P> std::enable_if_t<is_parser<P>::value, StringView> parse(P p) {
		const Cursor start = cursor;
		if (p.parse(cursor)) {
			return cursor - start;
		}
		else {
			return StringView();
		}
	}
	template <class F> std::enable_if_t<is_char_class<F>::value, StringView> parse(F f) {
		return parse(get_parser(f));
	}
	bool parse(char c) {
		return parse(get_parser(c));
	}
	bool parse(const StringView& s) {
		return parse(get_parser(s));
	}
	bool parse(const char* s) {
		return parse(get_parser(s));
	}
	template <class F> bool parse_not(F f) {
		return parse(sequence(not_(f), peek(any_char)));
	}
	template <class F> StringView parse_all(F f) {
		return parse(zero_or_more(f));
	}
	Parser(const SourceFile* file): cursor(file) {}
	Parser(const Cursor& cursor): cursor(cursor) {}
	const char* get_path() const {
		return cursor.get_path();
	}
	std::size_t get_position() const {
		return cursor.get_position();
	}
};

class MoebiusParser: private Parser {
	using SourcePosition = std::size_t;
	Program* program;
	Scope* current_scope = nullptr;
	static constexpr auto keyword(const StringView& s) {
		return sequence(s, not_(alphanumeric));
	}
	template <class T> [[noreturn]] void error(std::size_t position, const T& t) {
		print_error(Printer(std::cerr), get_path(), position, t);
		std::exit(EXIT_FAILURE);
	}
	template <class T> [[noreturn]] void error(const T& t) {
		error(get_position(), t);
	}
	void expect(const StringView& s) {
		if (!parse(s)) {
			error(format("expected \"%\"", s));
		}
	}
	void expect_keyword(const StringView& s) {
		if (!parse(keyword(s))) {
			error(format("expected \"%\"", s));
		}
	}
	bool parse_comment() {
		if (parse("//")) {
			parse(zero_or_more(sequence(not_("\n"), any_char)));
			return true;
		}
		if (parse("/*")) {
			parse(zero_or_more(sequence(not_("*/"), any_char)));
			expect("*/");
			return true;
		}
		return false;
	}
	void parse_white_space() {
		parse(zero_or_more(white_space));
		while (parse_comment()) {
			parse(zero_or_more(white_space));
		}
	}
	char parse_character() {
		if (parse("\\")) {
			if (!parse(peek(any_char))) {
				error("unexpected end");
			}
			char c = parse(any_char)[0];
			if (c == 'n') c = '\n';
			else if (c == 'r') c = '\r';
			else if (c == 't') c = '\t';
			else if (c == 'v') c = '\v';
			else if (c == '\'' || c == '\"' || c == '\\' || c == '$') c = c;
			else error("invalid escape");
			return c;
		}
		else {
			return parse(any_char)[0];
		}
	}
	const Expression* parse_string_segment() {
		const SourcePosition position = get_position();
		constexpr auto string_segment_end_char = [](char c) constexpr {
			return c == '"' || c == '$';
		};
		if (parse("$")) {
			const Expression* expression;
			if (parse("{")) {
				parse_white_space();
				expression = parse_expression();
				parse_white_space();
				expect("}");
			}
			else {
				StringView identifier = parse_identifier();
				expression = current_scope->look_up(identifier);
				if (expression == nullptr) {
					error(position, format("undefined variable \"%\"", identifier));
				}
			}
			const Expression* function = current_scope->look_up("toString");
			if (function == nullptr) {
				error(position, "function toString not defined");
			}
			ClosureCall* call = current_scope->create<ClosureCall>(function);
			call->set_position(position);
			call->add_argument(expression);
			return call;
		}
		else {
			std::string string;
			while (parse(sequence(not_(string_segment_end_char), peek(any_char)))) {
				string.push_back(parse_character());
			}
			StringLiteral* string_literal = current_scope->create<StringLiteral>(string);
			//string_literal->set_position(position);
			return string_literal;
		}
	}
	StringView parse_identifier() {
		if (!parse(peek(alphabetic))) {
			error("expected an identifier");
		}
		return parse(zero_or_more(alphanumeric));
	}
	const BinaryOperator* parse_binary_operator(const OperatorLevel* level) {
		for (const BinaryOperator& op: *level) {
			if (parse(sequence(op.string, not_(operator_char)))) {
				return &op;
			}
		}
		return nullptr;
	}
	const UnaryOperator* parse_unary_operator() {
		for (const UnaryOperator& op: unary_operators) {
			if (parse(sequence(op.string, not_(operator_char)))) {
				return &op;
			}
		}
		return nullptr;
	}
	const char* parse_intrinsic_name() {
		StringView name = parse_identifier();
		for (const char* intrinsic_name: intrinsics) {
			if (name == intrinsic_name) {
				return intrinsic_name;
			}
		}
		error(format("unknown intrinsic \"%\"", name));
	}
	const Expression* parse_expression_last() {
		const SourcePosition position = get_position();
		if (parse("{")) {
			parse_white_space();
			if (parse(peek(keyword("let"))) || parse(peek(keyword("return"))) || parse(peek(keyword("func"))) || parse(peek(keyword("struct"))) || parse(peek(keyword("enum")))) {
				const Expression* expression = parse_scope();
				parse_white_space();
				expect("}");
				return expression;
			}
			else {
				StructTypeDeclaration* struct_type_declaration = current_scope->create<StructTypeDeclaration>();
				StructTypeDefinition* struct_type_definition = new StructTypeDefinition(struct_type_declaration);
				StructLiteral* struct_literal = new StructLiteral(struct_type_definition);
				struct_type_definition->set_position(position);
				struct_literal->set_position(position);
				while (parse(not_("}"))) {
					const StringView field_name = parse_identifier();
					parse_white_space();
					const Expression* field;
					if (parse(":")) {
						parse_white_space();
						field = parse_expression();
						parse_white_space();
					}
					else {
						field = current_scope->look_up(field_name);
						if (field == nullptr) {
							error(format("undefined variable \"%\"", field_name));
						}
					}
					Intrinsic* field_type = current_scope->create<Intrinsic>("typeOf");
					field_type->add_argument(field);
					struct_type_definition->add_field(field_name, field_type);
					struct_literal->add_field(field_name, field);
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect("}");
				current_scope->add_expression(struct_type_definition);
				current_scope->add_expression(struct_literal);
				return struct_literal;
			}
		}
		else if (parse("(")) {
			parse_white_space();
			std::vector<const Expression*> elements;
			while (parse(not_(")"))) {
				elements.push_back(parse_expression());
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect(")");
			if (elements.size() == 1) {
				return elements[0];
			}
			else {
				TupleLiteral* tuple = current_scope->create<TupleLiteral>();
				tuple->set_position(position);
				for (const Expression* element: elements) {
					tuple->add_element(element);
				}
				return tuple;
			}
		}
		else if (parse(keyword("if"))) {
			parse_white_space();
			expect("(");
			parse_white_space();
			const Expression* condition = parse_expression();
			parse_white_space();
			expect(")");
			parse_white_space();
			If* if_ = new If(condition);
			if_->set_position(position);
			{
				Scope scope(current_scope, if_->get_then_block());
				const Expression* then_expression = parse_expression();
				current_scope->create<Return>(then_expression);
			}
			parse_white_space();
			expect_keyword("else");
			parse_white_space();
			{
				Scope scope(current_scope, if_->get_else_block());
				const Expression* else_expression = parse_expression();
				current_scope->create<Return>(else_expression);
			}
			current_scope->add_expression(if_);
			return if_;
		}
		else if (parse(keyword("switch"))) {
			parse_white_space();
			expect("(");
			parse_white_space();
			const Expression* enum_ = parse_expression();
			parse_white_space();
			expect(")");
			parse_white_space();
			expect("{");
			parse_white_space();
			Switch* switch_ = new Switch(enum_);
			switch_->set_position(position);
			while (parse(not_("}"))) {
				const StringView case_name = parse_identifier();
				parse_white_space();
				expect(":");
				parse_white_space();
				Scope scope(current_scope, switch_->add_case(case_name));
				current_scope->add_variable(case_name, current_scope->create<CaseVariable>());
				const Expression* case_expression = parse_expression();
				current_scope->create<Return>(case_expression);
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("}");
			current_scope->add_expression(switch_);
			return switch_;
		}
		else if (parse(keyword("func"))) {
			parse_white_space();
			expect("(");
			parse_white_space();
			Function* function = new Function();
			function->set_path(get_path());
			program->add_function(function);
			Closure* closure = new Closure(function);
			closure->set_position(position);
			{
				Scope scope(current_scope, closure, function->get_block());
				current_scope->set_self(current_scope->create<Argument>(function->add_argument()));
				while (parse(not_(")"))) {
					auto [argument_name, type_assert_position, argument_type] = parse_name();
					const Expression* argument = current_scope->create<Argument>(function->add_argument());
					current_scope->add_variable(argument_name, argument);
					if (argument_type) {
						TypeAssert* type_assert = current_scope->create<TypeAssert>(argument, argument_type);
						type_assert->set_position(type_assert_position);
					}
					parse_white_space();
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect(")");
				parse_white_space();
				const SourcePosition return_type_position = get_position();
				if (parse(":")) {
					parse_white_space();
					const Expression* type = parse_expression();
					ReturnType* return_type = current_scope->create<ReturnType>(type);
					return_type->set_position(return_type_position);
					parse_white_space();
				}
				expect("=>");
				parse_white_space();
				const Expression* expression = parse_expression();
				current_scope->create<Return>(expression);
			}
			current_scope->add_expression(closure);
			return closure;
		}
		else if (parse(keyword("struct"))) {
			parse_white_space();
			expect("{");
			parse_white_space();
			StructTypeDeclaration* struct_type_declaration = current_scope->create<StructTypeDeclaration>();
			StructTypeDefinition* struct_type_definition = new StructTypeDefinition(struct_type_declaration);
			struct_type_definition->set_position(position);
			while (parse(not_("}"))) {
				const StringView field_name = parse_identifier();
				parse_white_space();
				expect(":");
				parse_white_space();
				const Expression* field_type = parse_expression();
				struct_type_definition->add_field(field_name, field_type);
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("}");
			current_scope->add_expression(struct_type_definition);
			return struct_type_definition;
		}
		else if (parse(keyword("enum"))) {
			parse_white_space();
			expect("{");
			parse_white_space();
			EnumTypeDeclaration* enum_type_declaration = current_scope->create<EnumTypeDeclaration>();
			EnumTypeDefinition* enum_type_definition = new EnumTypeDefinition(enum_type_declaration);
			enum_type_definition->set_position(position);
			while (parse(not_("}"))) {
				const StringView case_name = parse_identifier();
				parse_white_space();
				if (parse(":")) {
					parse_white_space();
					const Expression* case_type = parse_expression();
					parse_white_space();
					enum_type_definition->add_case(case_name, case_type);
				}
				else {
					const Expression* case_type = current_scope->create<TypeLiteral>(TypeInterner::get_void_type());
					enum_type_definition->add_case(case_name, case_type);
				}
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("}");
			current_scope->add_expression(enum_type_definition);
			return enum_type_definition;
		}
		else if (parse("\"")) {
			const Expression* left = parse_string_segment();
			while (parse(sequence(not_("\""), peek(any_char)))) {
				const Expression* right = parse_string_segment();
				Intrinsic* intrinsic = current_scope->create<Intrinsic>("stringPush");
				intrinsic->add_argument(left);
				intrinsic->add_argument(right);
				left = intrinsic;
			}
			expect("\"");
			return left;
		}
		else if (parse("'")) {
			if (!parse(peek(any_char))) {
				error("unexpected end");
			}
			IntLiteral* int_literal = current_scope->create<IntLiteral>(parse_character());
			int_literal->set_position(position);
			expect("'");
			return int_literal;
		}
		else if (parse("[")) {
			parse_white_space();
			ArrayLiteral* array_literal = new ArrayLiteral();
			array_literal->set_position(position);
			while (parse(not_("]"))) {
				array_literal->add_element(parse_expression());
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect("]");
			current_scope->add_expression(array_literal);
			return array_literal;
		}
		else if (parse(keyword("false"))) {
			Expression* expression = current_scope->create<IntLiteral>(0);
			expression->set_position(position);
			return expression;
		}
		else if (parse(keyword("true"))) {
			Expression* expression = current_scope->create<IntLiteral>(1);
			expression->set_position(position);
			return expression;
		}
		else if (parse(keyword("void"))) {
			Expression* expression = current_scope->create<VoidLiteral>();
			expression->set_position(position);
			return expression;
		}
		else if (parse(keyword("Int"))) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_int_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse(keyword("String"))) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_string_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse(keyword("StringIterator"))) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_string_iterator_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse(keyword("Void"))) {
			Expression* expression = current_scope->create<TypeLiteral>(TypeInterner::get_void_type());
			expression->set_position(position);
			return expression;
		}
		else if (parse(peek(numeric))) {
			std::int32_t number = 0;
			for (char c: parse(zero_or_more(numeric))) {
				number *= 10;
				number += c - '0';
			}
			Expression* expression = current_scope->create<IntLiteral>(number);
			expression->set_position(position);
			return expression;
		}
		else if (parse(peek(alphabetic))) {
			StringView identifier = parse_identifier();
			const Expression* expression = current_scope->look_up(identifier);
			if (expression == nullptr) {
				error(position, format("undefined variable \"%\"", identifier));
			}
			return expression;
		}
		else if (parse("@")) {
			const char* name = parse_intrinsic_name();
			parse_white_space();
			expect("(");
			parse_white_space();
			Intrinsic* intrinsic = new Intrinsic(name);
			intrinsic->set_position(position);
			while (parse(not_(")"))) {
				intrinsic->add_argument(parse_expression());
				parse_white_space();
				if (!parse(",")) {
					break;
				}
				parse_white_space();
			}
			expect(")");
			current_scope->add_expression(intrinsic);
			return intrinsic;
		}
		else {
			error("expected an expression");
		}
	}
	const Expression* parse_expression(const OperatorLevel* level = operators.begin()) {
		if (level == operators.end()) {
			const SourcePosition position = get_position();
			if (const UnaryOperator* op = parse_unary_operator()) {
				parse_white_space();
				Expression* expression = op->create(parse_expression(level));
				expression->set_position(position);
				current_scope->add_expression(expression);
				return expression;
			}
			const Expression* expression = parse_expression_last();
			parse_white_space();
			while (true) {
				const SourcePosition position = get_position();
				if (parse("(")) {
					parse_white_space();
					ClosureCall* call = new ClosureCall(expression);
					call->set_position(position);
					while (parse(not_(")"))) {
						call->add_argument(parse_expression());
						parse_white_space();
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect(")");
					current_scope->add_expression(call);
					expression = call;
					parse_white_space();
				}
				else if (parse(".")) {
					parse_white_space();
					StringView name = parse_identifier();
					parse_white_space();
					const SourcePosition method_call_position = get_position();
					if (parse("(")) {
						parse_white_space();
						const Expression* method = current_scope->look_up(name);
						MethodCall* call = new MethodCall(expression, name, method);
						call->set_position(method_call_position);
						while (parse(not_(")"))) {
							call->add_argument(parse_expression());
							parse_white_space();
							if (!parse(",")) {
								break;
							}
							parse_white_space();
						}
						expect(")");
						current_scope->add_expression(call);
						expression = call;
						parse_white_space();
					}
					else {
						StructAccess* struct_access = current_scope->create<StructAccess>(expression, name);
						struct_access->set_position(position);
						expression = struct_access;
					}
				}
				else if (parse("{")) {
					parse_white_space();
					StructLiteral* struct_literal = new StructLiteral(expression);
					struct_literal->set_position(position);
					while (parse(not_("}"))) {
						const StringView field_name = parse_identifier();
						parse_white_space();
						const Expression* field;
						if (parse(":")) {
							parse_white_space();
							field = parse_expression();
							parse_white_space();
						}
						else {
							field = current_scope->look_up(field_name);
							if (field == nullptr) {
								error(format("undefined variable \"%\"", field_name));
							}
						}
						struct_literal->add_field(field_name, field);
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect("}");
					current_scope->add_expression(struct_literal);
					TypeAssert* type_assert = current_scope->create<TypeAssert>(struct_literal, expression);
					type_assert->set_position(position);
					expression = struct_literal;
					parse_white_space();
				}
				else {
					break;
				}
			}
			return expression;
		}
		const Expression* left = parse_expression(level + 1);
		parse_white_space();
		SourcePosition position = get_position();
		while (const BinaryOperator* op = parse_binary_operator(level)) {
			parse_white_space();
			const Expression* right = parse_expression(level + 1);
			Expression* expression = op->create(left, right);
			expression->set_position(position);
			current_scope->add_expression(expression);
			left = expression;
			parse_white_space();
			position = get_position();
		}
		return left;
	}
	std::tuple<StringView, SourcePosition, const Expression*> parse_name() {
		const StringView name = parse_identifier();
		parse_white_space();
		const SourcePosition type_assert_position = get_position();
		if (parse(":")) {
			parse_white_space();
			const Expression* type = parse_expression();
			return std::make_tuple(name, type_assert_position, type);
		}
		else {
			return std::make_tuple(name, type_assert_position, nullptr);
		}
	}
	const Expression* parse_scope() {
		Scope scope(current_scope);
		while (true) {
			const SourcePosition position = get_position();
			if (parse(keyword("let"))) {
				parse_white_space();
				std::vector<std::tuple<StringView, SourcePosition, const Expression*>> element_names;
				if (parse("(")) {
					parse_white_space();
					while (parse(not_(")"))) {
						element_names.push_back(parse_name());
						parse_white_space();
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect(")");
				}
				else {
					element_names.push_back(parse_name());
				}
				parse_white_space();
				expect("=");
				parse_white_space();
				const Expression* expression = parse_expression();
				if (element_names.size() == 1) {
					auto [name, type_assert_position, type] = element_names[0];
					if (type) {
						TypeAssert* type_assert = current_scope->create<TypeAssert>(expression, type);
						type_assert->set_position(type_assert_position);
					}
					current_scope->add_variable(name, expression);
				}
				else for (std::size_t i = 0; i < element_names.size(); ++i) {
					auto [name, type_assert_position, type] = element_names[i];
					TupleAccess* tuple_access = current_scope->create<TupleAccess>(expression, i);
					tuple_access->set_position(position);
					if (type) {
						TypeAssert* type_assert = current_scope->create<TypeAssert>(tuple_access, type);
						type_assert->set_position(type_assert_position);
					}
					current_scope->add_variable(name, tuple_access);
				}
				parse_white_space();
			}
			else if (parse(keyword("func"))) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("(");
				parse_white_space();
				Function* function = new Function();
				function->set_path(get_path());
				program->add_function(function);
				Closure* closure = new Closure(function);
				closure->set_position(position);
				{
					Scope scope(current_scope, closure, function->get_block());
					const Expression* self = current_scope->create<Argument>(function->add_argument());
					current_scope->set_self(self);
					current_scope->add_variable(name, self);
					while (parse(not_(")"))) {
						auto [argument_name, type_assert_position, argument_type] = parse_name();
						const Expression* argument = current_scope->create<Argument>(function->add_argument());
						current_scope->add_variable(argument_name, argument);
						if (argument_type) {
							TypeAssert* type_assert = current_scope->create<TypeAssert>(argument, argument_type);
							type_assert->set_position(type_assert_position);
						}
						parse_white_space();
						if (!parse(",")) {
							break;
						}
						parse_white_space();
					}
					expect(")");
					parse_white_space();
					const SourcePosition return_type_position = get_position();
					if (parse(":")) {
						parse_white_space();
						const Expression* type = parse_expression();
						ReturnType* return_type = current_scope->create<ReturnType>(type);
						return_type->set_position(return_type_position);
						parse_white_space();
					}
					expect("=>");
					parse_white_space();
					const Expression* expression = parse_expression();
					current_scope->create<Return>(expression);
				}
				current_scope->add_expression(closure);
				current_scope->add_variable(name, closure);
				parse_white_space();
			}
			else if (parse(keyword("struct"))) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("{");
				parse_white_space();
				StructTypeDeclaration* struct_type_declaration = current_scope->create<StructTypeDeclaration>();
				current_scope->add_variable(name, struct_type_declaration);
				StructTypeDefinition* struct_type_definition = new StructTypeDefinition(struct_type_declaration);
				struct_type_definition->set_position(position);
				while (parse(not_("}"))) {
					const StringView field_name = parse_identifier();
					parse_white_space();
					expect(":");
					parse_white_space();
					const Expression* field_type = parse_expression();
					struct_type_definition->add_field(field_name, field_type);
					parse_white_space();
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect("}");
				parse_white_space();
				current_scope->add_expression(struct_type_definition);
			}
			else if (parse(keyword("enum"))) {
				parse_white_space();
				const StringView name = parse_identifier();
				parse_white_space();
				expect("{");
				parse_white_space();
				EnumTypeDeclaration* enum_type_declaration = current_scope->create<EnumTypeDeclaration>();
				current_scope->add_variable(name, enum_type_declaration);
				EnumTypeDefinition* enum_type_definition = new EnumTypeDefinition(enum_type_declaration);
				enum_type_definition->set_position(position);
				while (parse(not_("}"))) {
					const StringView case_name = parse_identifier();
					parse_white_space();
					if (parse(":")) {
						parse_white_space();
						const Expression* case_type = parse_expression();
						parse_white_space();
						enum_type_definition->add_case(case_name, case_type);
					}
					else {
						const Expression* case_type = current_scope->create<TypeLiteral>(TypeInterner::get_void_type());
						enum_type_definition->add_case(case_name, case_type);
					}
					if (!parse(",")) {
						break;
					}
					parse_white_space();
				}
				expect("}");
				parse_white_space();
				current_scope->add_expression(enum_type_definition);
			}
			else {
				break;
			}
		}
		expect_keyword("return");
		parse_white_space();
		return parse_expression();
	}
public:
	MoebiusParser(const SourceFile* file, Program* program): Parser(file), program(program) {}
	const Function* parse_program() {
		parse_white_space();
		Function* main_function = new Function();
		main_function->set_path(get_path());
		program->add_function(main_function);
		Scope scope(current_scope, main_function->get_block());
		const Expression* expression = parse_scope();
		current_scope->create<Return>(expression);
		parse_white_space();
		if (parse(peek(any_char))) {
			error("unexpected character at end of program");
		}
		return main_function;
	}
	static const Function* parse_program(const char* path, Program* program) {
		SourceFile file(path);
		MoebiusParser parser(&file, program);
		return parser.parse_program();
	}
};
