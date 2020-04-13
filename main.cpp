#include "parser.hpp"
#include "codegen.hpp"
#include <string>

int main(int argc, char** argv) {
	SourceFile file(argv[1]);
	const Expression* expr = parse(file);
	if (expr) {
		codegen(expr, (std::string(argv[1]) + ".exe").c_str());
	}
}
