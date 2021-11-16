dist/index.html: main.cpp parser.hpp passes.hpp codegen_js.hpp printer.hpp shell.html
	em++ -std=c++17 -O2 --bind --shell-file shell.html -o $@ $<

run:
	python3 -m http.server -d dist

.PHONY: run
