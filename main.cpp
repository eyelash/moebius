#include "parser.hpp"

int main(int argc, char** argv) {
	if (argc > 1) {
		const Expression* program = MoebiusParser::parse_program(argv[1]);
		delete program;
	}
}
