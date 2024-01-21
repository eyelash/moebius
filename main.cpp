#include "parser.hpp"
#include "interpreter.hpp"

int main(int argc, char** argv) {
	if (argc > 1) {
		Result<Reference<Expression>> result = MoebiusParser::parse_program(argv[1]);
		if (result.index() != 0) {
			print(std::cerr, ErrorPrinter(&std::get<1>(result)));
			return EXIT_FAILURE;
		}
		Expression* program = std::get<0>(result);
		Interpreter::interpret_program(program);
	}
}
