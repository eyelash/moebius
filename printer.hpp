#pragma once

#include "ast.hpp"
#include <cstddef>
#include <iostream>
#include <vector>
#include <fstream>
#include <iterator>

class Printer {
	std::ostream& ostream;
	mutable unsigned int indentation = 0;
	mutable bool is_at_bol = true;
public:
	Printer(std::ostream& ostream = std::cout): ostream(ostream) {}
	void print(char c) const {
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
	void print(const StringView& s) const {
		for (char c: s) {
			print(c);
		}
	}
	void print(const char* s) const {
		print(StringView(s));
	}
	void print(const std::string& s) const {
		print(StringView(s.data(), s.size()));
	}
	template <class T> void print(const T& t) const {
		t.print(*this);
	}
	template <class T> void println(const T& t) const {
		print(t);
		print('\n');
	}
	template <class T> void println_indented(const T& t) const {
		++indentation;
		println(t);
		--indentation;
	}
};

template <class F> class PrintFunctor {
	F f;
public:
	constexpr PrintFunctor(F f): f(f) {}
	void print(const Printer& p) const {
		f(p);
	}
};
template <class F> constexpr PrintFunctor<F> print_functor(F f) {
	return PrintFunctor<F>(f);
}

template <class... T> class PrintTuple;
template <> class PrintTuple<> {
public:
	constexpr PrintTuple() {}
	void print(const Printer& p) const {}
	void print_formatted(const Printer& p, const char* s) const {
		p.print(s);
	}
};
template <class T0, class... T> class PrintTuple<T0, T...> {
	const T0& t0;
	PrintTuple<T...> t;
public:
	constexpr PrintTuple(const T0& t0, const T&... t): t0(t0), t(t...) {}
	void print(const Printer& p) const {
		p.print(t0);
		p.print(t);
	}
	void print_formatted(const Printer& p, const char* s) const {
		while (*s) {
			if (*s == '%') {
				++s;
				if (*s != '%') {
					p.print(t0);
					t.print_formatted(p, s);
					return;
				}
			}
			p.print(*s);
			++s;
		}
	}
};
template <class... T> constexpr PrintTuple<T...> print_tuple(const T&... t) {
	return PrintTuple<T...>(t...);
}

template <class... T> class Format {
	PrintTuple<T...> t;
	const char* s;
public:
	constexpr Format(const char* s, const T&... t): t(t...), s(s) {}
	void print(const Printer& p) const {
		t.print_formatted(p, s);
	}
};
template <class... T> constexpr Format<T...> format(const char* s, const T&... t) {
	return Format<T...>(s, t...);
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

class PrintNumber {
	unsigned int n;
public:
	constexpr PrintNumber(unsigned int n): n(n) {}
	void print(const Printer& p) const {
		if (n >= 10) {
			p.print(PrintNumber(n / 10));
		}
		p.print(static_cast<char>('0' + n % 10));
	}
};
constexpr PrintNumber print_number(unsigned int n) {
	return PrintNumber(n);
}

class PrintHexadecimal {
	unsigned int n;
	unsigned int digits;
	static constexpr char get_hex(unsigned int c) {
		return c < 10 ? '0' + c : 'A' + (c - 10);
	}
public:
	constexpr PrintHexadecimal(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(const Printer& p) const {
		if (n >= 16 || digits > 1) {
			p.print(PrintHexadecimal(n / 16, digits > 1 ? digits - 1 : digits));
		}
		p.print(get_hex(n % 16));
	}
};
constexpr PrintHexadecimal print_hexadecimal(unsigned int n, unsigned int digits = 1) {
	return PrintHexadecimal(n, digits);
}
template <class T> constexpr PrintHexadecimal print_pointer(const T* ptr) {
	return PrintHexadecimal(reinterpret_cast<std::size_t>(ptr));
}

class PrintOctal {
	unsigned int n;
	unsigned int digits;
public:
	constexpr PrintOctal(unsigned int n, unsigned int digits = 1): n(n), digits(digits) {}
	void print(const Printer& p) const {
		if (n >= 8 || digits > 1) {
			p.print(PrintOctal(n / 8, digits > 1 ? digits - 1 : digits));
		}
		p.print(static_cast<char>('0' + n % 8));
	}
};
constexpr PrintOctal print_octal(unsigned int n, unsigned int digits = 1) {
	return PrintOctal(n, digits);
}

class PrintPlural {
	const char* word;
	unsigned int count;
public:
	constexpr PrintPlural(const char* word, unsigned int count): word(word), count(count) {}
	void print(const Printer& p) const {
		p.print(print_number(count));
		p.print(' ');
		p.print(word);
		if (count != 1) {
			p.print('s');
		}
	}
};

constexpr PrintPlural print_plural(const char* word, unsigned int count) {
	return PrintPlural(word, count);
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

template <class T, class C> void print_message(const Printer& printer, const C& color, const char* severity, const T& t) {
	printer.print(bold(color(format("%: ", severity))));
	printer.print(t);
	printer.print('\n');
}
template <class T, class C> void print_message(const Printer& printer, const char* path, std::size_t source_position, const C& color, const char* severity, const T& t) {
	if (path == nullptr) {
		print_message(printer, color, severity, t);
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

		printer.print(bold(format("%:%:%: ", path, print_number(line_number), print_number(column))));
		print_message(printer, color, severity, t);

		c = line_start;
		while (c < end && *c != '\n') {
			printer.print(*c);
			++c;
		}
		printer.print('\n');

		c = line_start;
		while (c < position) {
			printer.print(*c == '\t' ? '\t' : ' ');
			++c;
		}
		printer.print(bold(color('^')));
		printer.print('\n');
	}
}
template <class T> void print_error(const Printer& printer, const T& t) {
	print_message(printer, red, "error", t);
}
template <class T> void print_error(const Printer& printer, const char* path, std::size_t source_position, const T& t) {
	print_message(printer, path, source_position, red, "error", t);
}

class Variable {
	std::size_t index;
public:
	constexpr Variable(std::size_t index): index(index) {}
	Variable() {}
	void print(const Printer& printer) const {
		printer.print(format("v%", print_number(index)));
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
	void print(const Printer& p) const {
		class PrintExpressionVisitor: public Visitor<void> {
			const Printer& p;
		public:
			PrintExpressionVisitor(const Printer& p): p(p) {}
			void visit_int_literal(const IntLiteral& int_literal) override {
				p.print(print_number(int_literal.get_value()));
			}
			void visit_binary_expression(const BinaryExpression& binary_expression) override {
				p.print(format("(% % %)", PrintExpression(binary_expression.get_left()), print_operation(binary_expression.get_operation()), PrintExpression(binary_expression.get_right())));
			}
			void visit_if(const If& if_) override {
				p.println(format("if (%)", PrintExpression(if_.get_condition())));
				p.println_indented(PrintExpression(if_.get_then_expression()));
				p.println("else");
				p.println_indented(PrintExpression(if_.get_else_expression()));
			}
		};
		PrintExpressionVisitor visitor(p);
		expression->accept(visitor);
	}
};
