#include "parser.hpp"
#include <iostream>

int main(int argc, char** argv) {
	Parser parser(argv[1]);
	Expression* expr = parser.parse();
	std::int32_t value;
	expr->evaluate(&value);
	std::cout << value << std::endl;
}
