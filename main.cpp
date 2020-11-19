#include "parser.hpp"
#include "passes.hpp"
#include "codegen_x86.hpp"
#include "codegen_c.hpp"
#include "codegen_js.hpp"
#include <string>

int main(int argc, char** argv) {
	const Program* program = parse(argv[1]);
	program = Pass1::run(*program);
	program = Lowering::run(*program);
	program = Pass1::run(*program);
	program = Pass2::run(*program);
	CodegenX86::codegen(*program, (std::string(argv[1]) + ".exe").c_str());
}
