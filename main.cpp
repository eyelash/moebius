#include "parser.hpp"
#include "interpreter.hpp"

int main(int argc, char** argv) {
	if (argc > 1) {
		const Expression* program = MoebiusParser::parse_program(argv[1]);
		Interpreter::interpret_program(program);
		delete program;
	}
}
