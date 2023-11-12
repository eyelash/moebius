#pragma once

class Expression {
public:
	virtual ~Expression() = default;
};

class IntLiteral: public Expression {
	std::int32_t value;
public:
	IntLiteral(std::int32_t value): value(value) {}
	std::int32_t get_value() const {
		return value;
	}
};
