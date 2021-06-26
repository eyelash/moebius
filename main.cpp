#include "parser.hpp"
#include "passes.hpp"
#include "codegen_x86.hpp"
#include "codegen_c.hpp"
#include "codegen_js.hpp"
#include <string>

class Arguments {
public:
	const char* source_path = nullptr;
	void (*codegen)(const Program& program, const char* source_path) = CodegenC::codegen;
	void parse(int argc, char** argv) {
		for (int i = 1; i < argc; ++i) {
			if (StringView(argv[i]) == "-c") codegen = CodegenC::codegen;
			else if (StringView(argv[i]) == "-js") codegen = CodegenJS::codegen;
			else source_path = argv[i];
		}
	}
};

int main(int argc, char** argv) {
	Arguments arguments;
	arguments.parse(argc, argv);
	if (arguments.source_path == nullptr) {
		Printer printer(std::cerr);
		printer.print(bold(red("error: ")));
		printer.print("no input file");
		printer.print('\n');
		return EXIT_FAILURE;
	}
	std::unique_ptr<Program> program = parse(arguments.source_path);
	program = Pass1::run(*program);
	program = Pass2::run(*program);
	program = Pass1::run(*program);
	program = Pass3::run(*program);
	program = Pass1::run(*program);
	program = Pass4::run(*program);
	program = Pass1::run(*program);
	Pass5::run(*program);
	arguments.codegen(*program, arguments.source_path);
}
