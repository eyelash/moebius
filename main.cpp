#include "parser.hpp"
#include "passes.hpp"
#include "codegen_x86.hpp"
#include "codegen_c.hpp"
#include "codegen_js.hpp"
#include <string>

int main(int argc, char** argv) {
	std::unique_ptr<Program> program = parse(argv[1]);
	program = Pass1::run(*program);
	program = Pass2::run(*program);
	program = Pass1::run(*program);
	program = Pass3::run(*program);
	program = Pass1::run(*program);
	CodegenC::codegen(*program, (std::string(argv[1]) + ".exe").c_str());
}
