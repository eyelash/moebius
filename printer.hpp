#pragma once

#include "ast.hpp"
#include <cstddef>
#include <iostream>
#include <vector>
#include <fstream>
#include <iterator>

class PrintContext {
	std::ostream& ostream;
	unsigned int indentation = 0;
	bool is_at_bol = true;
public:
	PrintContext(std::ostream& ostream = std::cout): ostream(ostream) {}
	void print(char c) {
		if (c == '\n') {
			ostream.put('\n');
			is_at_bol = true;
		}
		else {
			if (is_at_bol) {
				for (unsigned int i = 0; i < indentation; ++i) {
					ostream.put('\t');
				}
				is_at_bol = false;
			}
			ostream.put(c);
		}
	}
	void increase_indentation() {
		++indentation;
	}
	void decrease_indentation() {
		--indentation;
	}
};

class CharPrinter {
	char c;
public:
	constexpr CharPrinter(char c): c(c) {}
	void print(PrintContext& context) const {
		context.print(c);
	}
};

class StringPrinter {
	StringView s;
public:
	constexpr StringPrinter(const StringView& s): s(s) {}
	constexpr StringPrinter(const char* s): s(s) {}
	StringPrinter(const std::string& s): s(s.data(), s.size()) {}
	void print(PrintContext& context) const {
		for (char c: s) {
			context.print(c);
		}
	}
};

template <class P, class = void> struct is_printer: std::false_type {};
template <class P> struct is_printer<P, decltype(std::declval<P>().print(std::declval<PrintContext&>()))>: std::true_type {};

constexpr CharPrinter get_printer(char c) {
	return CharPrinter(c);
}
constexpr StringPrinter get_printer(const StringView& s) {
	return StringPrinter(s);
}
constexpr StringPrinter get_printer(const char* s) {
	return StringPrinter(s);
}
StringPrinter get_printer(const std::string& s) {
	return StringPrinter(s);
}
template <class P> constexpr std::enable_if_t<is_printer<P>::value, P> get_printer(P p) {
	return p;
}

class Printer {
	mutable PrintContext context;
public:
	Printer(std::ostream& ostream = std::cout): context(ostream) {}
	template <class T> void print(const T& t) const {
		get_printer(t).print(context);
	}
	template <class T> void println(const T& t) const {
		print(t);
		print('\n');
	}
	template <class T> void println_indented(const T& t) const {
		context.increase_indentation();
		println(t);
		context.decrease_indentation();
	}
};

template <class P> void print(std::ostream& ostream, const P& p) {
	PrintContext context(ostream);
	p.print(context);
}
template <class P> void print(const P& p) {
	print(std::cout, p);
}

template <class P> class LnPrinter {
	P p;
public:
	constexpr LnPrinter(P p): p(p) {}
	void print(PrintContext& context) const {
		p.print(context);
		context.print('\n');
	}
};
template <class P> constexpr auto ln(P p) {
	return LnPrinter(get_printer(p));
}
constexpr CharPrinter ln() {
	return CharPrinter('\n');
}

template <class P> class IndentPrinter {
	P p;
public:
	constexpr IndentPrinter(P p): p(p) {}
	void print(PrintContext& c) const {
		c.increase_indentation();
		p.print(c);
		c.decrease_indentation();
	}
};
template <class P> constexpr auto indented(P p) {
	return IndentPrinter(get_printer(p));
}

template <class F> class PrintFunctor {
	F f;
public:
	constexpr PrintFunctor(F f): f(f) {}
	void print(PrintContext& context) const {
		f(context);
	}
};
template <class F> constexpr PrintFunctor<F> print_functor(F f) {
	return PrintFunctor(f);
}

template <class... T> class PrintTuple;
template <> class PrintTuple<> {
public:
	constexpr PrintTuple() {}
	void print(PrintContext& context) const {}
	void print_formatted(PrintContext& context, const char* s) const {
		get_printer(s).print(context);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	T0 t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(T0 t0, T... t): t0(t0), t(t...) {}
	void print(PrintContext& context) const {
		t0.print(context);
		t.print(context);
	}
	void print_formatted(PrintContext& context, const char* s) const {
		while (*s) {
			if (*s == '%') {
				++s;
				if (*s != '%') {
					t0.print(context);
					t.print_formatted(context, s);
					return;
				}
			}
			context.print(*s);
			++s;
		}
	}
};
template <class... T> PrintTuple(T...) -> PrintTuple<T...>;
template <class... T> constexpr auto print_tuple(T... t) {
	return PrintTuple(get_printer(t)...);
}

template <class... T> class Format {
	PrintTuple<T...> t;
	const char* s;
public:
	constexpr Format(const char* s, T... t): t(t...), s(s) {}
	void print(PrintContext& context) const {
		t.print_formatted(context, s);
	}
};
template <class... T> constexpr auto format(const char* s, T... t) {
	return Format(s, get_printer(t)...);
}

constexpr auto bold = [](const auto& t) {
	return print_tuple("\x1B[1m", t, "\x1B[22m");
};
constexpr auto red = [](const auto& t) {
	return print_tuple("\x1B[31m", t, "\x1B[39m");
};
constexpr auto green = [](const auto& t) {
	return print_tuple("\x1B[32m", t, "\x1B[39m");
};
constexpr auto yellow = [](const auto& t) {
	return print_tuple("\x1B[33m", t, "\x1B[39m");
};

class NumberPrinter {
	unsigned int n;
public:
	constexpr NumberPrinter(unsigned int n): n(n) {}
	void print(PrintContext& context) const {
		if (n >= 10) {
			NumberPrinter(n / 10).print(context);
		}
		context.print(static_cast<char>('0' + n % 10));
	}
};
constexpr NumberPrinter print_number(unsigned int n) {
	return NumberPrinter(n);
}

class HexadecimalPrinter {
	unsigned int n;
	unsigned int digits;
	static constexpr char get_hex(unsigned int c) {
		return c < 10 ? '0' + c : 'A' + (c - 10);
	}
public:
	constexpr HexadecimalPrinter(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(PrintContext& context) const {
		if (n >= 16 || digits > 1) {
			HexadecimalPrinter(n / 16, digits > 1 ? digits - 1 : digits).print(context);
		}
		context.print(get_hex(n % 16));
	}
};
constexpr HexadecimalPrinter print_hexadecimal(unsigned int n, unsigned int digits = 1) {
	return HexadecimalPrinter(n, digits);
}
template <class T> constexpr HexadecimalPrinter print_pointer(const T* ptr) {
	return HexadecimalPrinter(reinterpret_cast<std::size_t>(ptr));
}

class OctalPrinter {
	unsigned int n;
	unsigned int digits;
public:
	constexpr OctalPrinter(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(PrintContext& context) const {
		if (n >= 8 || digits > 1) {
			OctalPrinter(n / 8, digits > 1 ? digits - 1 : digits).print(context);
		}
		context.print(static_cast<char>('0' + n % 8));
	}
};
constexpr OctalPrinter print_octal(unsigned int n, unsigned int digits = 1) {
	return OctalPrinter(n, digits);
}

class PluralPrinter {
	const char* word;
	unsigned int count;
public:
	constexpr PluralPrinter(const char* word, unsigned int count): word(word), count(count) {}
	void print(PrintContext& context) const {
		print_number(count).print(context);
		context.print(' ');
		get_printer(word).print(context);
		if (count != 1) {
			context.print('s');
		}
	}
};

constexpr PluralPrinter print_plural(const char* word, unsigned int count) {
	return PluralPrinter(word, count);
}

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

template <class P, class C> void print_message(PrintContext& context, const C& color, const char* severity, const P& p) {
	bold(color(format("%: ", severity))).print(context);
	p.print(context);
	context.print('\n');
}
template <class P, class C> void print_message(PrintContext& context, const char* path, std::size_t source_position, const C& color, const char* severity, const P& p) {
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

		bold(format("%:%:%: ", path, print_number(line_number), print_number(column))).print(context);
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
		bold(color('^')).print(context);
		context.print('\n');
	}
}

template <class P> class Error {
	const char* path;
	std::size_t source_position;
	P p;
public:
	constexpr Error(const char* path, std::size_t source_position, P p): path(path), source_position(source_position), p(p) {}
	void print(PrintContext& context) const {
		print_message(context, path, source_position, red, "error", p);
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
	void print(PrintContext& context) const {
		class PrintExpressionVisitor: public Visitor<void> {
			PrintContext& context;
		public:
			PrintExpressionVisitor(PrintContext& context): context(context) {}
			void visit_int_literal(const IntLiteral& int_literal) override {
				print_number(int_literal.get_value()).print(context);
			}
			void visit_binary_expression(const BinaryExpression& binary_expression) override {
				format("(% % %)", PrintExpression(binary_expression.get_left()), print_operation(binary_expression.get_operation()), PrintExpression(binary_expression.get_right())).print(context);
			}
			void visit_if(const If& if_) override {
				ln(format("if (%)", PrintExpression(if_.get_condition()))).print(context);
				indented(ln(PrintExpression(if_.get_then_expression()))).print(context);
				ln("else").print(context);
				indented(ln(PrintExpression(if_.get_else_expression()))).print(context);
			}
		};
		PrintExpressionVisitor visitor(context);
		expression->accept(visitor);
	}
};
