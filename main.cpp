#include "parser.hpp"
#include "codegen.hpp"
#include <string>

int main(int argc, char** argv) {
	const Expression* expr = parse(argv[1]);
	if (expr) {
		codegen(expr, (std::string(argv[1]) + ".exe").c_str());
	}
}
