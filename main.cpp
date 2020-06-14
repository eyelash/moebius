#include "parser.hpp"
#include "codegen.hpp"
#include <string>

int main(int argc, char** argv) {
	const Function* main_function = parse(argv[1]);
	main_function = Pass1::run(main_function);
	main_function = Pass2::run(main_function);
	CodegenX86::codegen(main_function, (std::string(argv[1]) + ".exe").c_str());
}
