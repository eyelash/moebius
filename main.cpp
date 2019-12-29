#include "parser.hpp"
#include "codegen.hpp"
#include <vector>
#include <fstream>
#include <string>
#include <iostream>

std::vector<char> read_file(const char* file_name) {
	std::ifstream file(file_name);
	return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
	std::vector<char> file = read_file(argv[1]);
	const Expression* expr = parse(StringView(file.data(), file.size()));
	if (expr) {
		codegen(expr, (std::string(argv[1]) + ".exe").c_str());
	}
}
