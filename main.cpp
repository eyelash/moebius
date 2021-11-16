#include "parser.hpp"
#include "passes.hpp"
#include "codegen_js.hpp"
#include <string>
#include <emscripten/bind.h>

void compile(std::string source) {
	{
		std::ofstream file("input.moeb");
		file << source;
	}
	std::unique_ptr<Program> program = MoebiusParser::parse_program("input.moeb");
	program = Pass1::run(*program);
	program = Lowering::run(*program);
	program = Pass3::run(*program);
	program = DeadCodeElimination::run(*program);
	program = Pass2::run(*program);
	program = Pass1::run(*program);
	program = DeadCodeElimination::run(*program);
	program = Pass4::run(*program);
	TailCallData tail_call_data;
	Pass5::run(*program, tail_call_data);
	CodegenJS::codegen(*program, "input.moeb", tail_call_data);
}

EMSCRIPTEN_BINDINGS(moebius) {
	emscripten::function("compile", &compile);
}
