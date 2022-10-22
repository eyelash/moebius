#pragma once

#include "printer.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>
#include <fstream>

enum Register: std::uint8_t {
	EAX,
	ECX,
	EDX,
	EBX,
	ESP,
	EBP,
	ESI,
	EDI
};

class Ptr {
	Register r;
	std::uint32_t offset;
public:
	constexpr Ptr(Register r, std::uint32_t offset): r(r), offset(offset) {}
	constexpr Register get_register() const {
		return r;
	}
	constexpr std::uint32_t get_offset() const {
		return offset;
	}
};

constexpr Ptr PTR(Register r, std::uint32_t offset = 0) {
	return Ptr(r, offset);
}

class Assembler {
	using Addr = std::uint32_t;
	static constexpr Addr VADDR = 0x10000;
	static constexpr Addr ELF_HEADER_SIZE = 52;
	static constexpr Addr PROGRAM_HEADER_SIZE = 32;
	std::vector<char> data;
	template <class T> void write(T t) {
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			data.push_back(t & 0xFF);
			t >>= 8;
		}
	}
	template <class T> void write(std::size_t position, T t) {
		auto iterator = data.begin() + position;
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			*iterator++ = t & 0xFF;
			t >>= 8;
		}
	}
	void opcode(std::uint8_t opcode) {
		write<std::uint8_t>(opcode);
	}
	void opcode_0F(std::uint8_t opcode) {
		write<std::uint8_t>(0x0F);
		write<std::uint8_t>(opcode);
	}
	void ModRM(std::uint8_t mod, std::uint8_t reg, std::uint8_t rm) {
		write<std::uint8_t>(mod << 6 | reg << 3 | rm);
	}
	void SIB(std::uint8_t scale, std::uint8_t index, std::uint8_t base) {
		write<std::uint8_t>(scale << 6 | index << 3 | base);
	}
	void operand(std::uint8_t op1, Register op2) {
		ModRM(3, op1, op2);
	}
	void operand(std::uint8_t op1, Ptr op2) {
		const std::uint32_t offset = op2.get_offset();
		if (offset == 0 && op2.get_register() != EBP) {
			ModRM(0, op1, op2.get_register());
			if (op2.get_register() == ESP) {
				SIB(0, ESP, ESP);
			}
		}
		else if (offset + 128 < 256) {
			ModRM(1, op1, op2.get_register());
			if (op2.get_register() == ESP) {
				SIB(0, ESP, ESP);
			}
			write<std::uint8_t>(offset);
		}
		else {
			ModRM(2, op1, op2.get_register());
			if (op2.get_register() == ESP) {
				SIB(0, ESP, ESP);
			}
			write<std::uint32_t>(offset);
		}
	}
public:
	class Jump {
		std::size_t position;
	public:
		constexpr Jump(std::size_t position): position(position) {}
		void set_target(Assembler& assembler, std::size_t target) const {
			assembler.write<std::uint32_t>(position, target - (position + 4));
		}
	};
	void write_elf_header() {
		write<std::uint8_t>(0x7f);
		write<std::uint8_t>('E');
		write<std::uint8_t>('L');
		write<std::uint8_t>('F');
		write<std::uint8_t>(1); // ELFCLASS32
		write<std::uint8_t>(1); // ELFDATA2LSB
		write<std::uint8_t>(1); // version
		write<std::uint8_t>(0); // OS ABI
		write<std::uint8_t>(0); // ABI version
		while (data.size() < 16) {
			write<std::uint8_t>(0);
		}

		write<std::uint16_t>(2); // ET_EXEC
		write<std::uint16_t>(3); // EM_386
		write<std::uint32_t>(1);
		write<Addr>(VADDR + ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE); // entry point
		write<Addr>(ELF_HEADER_SIZE); // program header position
		write<Addr>(0); // section header position
		write<std::uint32_t>(0); // flags
		write<std::uint16_t>(ELF_HEADER_SIZE); // ELF header size
		write<std::uint16_t>(PROGRAM_HEADER_SIZE); // program header size
		write<std::uint16_t>(1); // number of program headers
		write<std::uint16_t>(0); // section header size
		write<std::uint16_t>(0); // number of section headers
		write<std::uint16_t>(0);
	}
	void write_program_header() {
		write<std::uint32_t>(1); // PT_LOAD
		write<Addr>(0); // offset
		write<Addr>(VADDR); // vaddr
		write<Addr>(0); // paddr
		write<std::uint32_t>(0); // filesz, will be set later
		write<std::uint32_t>(0); // memsz, will be set later
		write<std::uint32_t>(5); // flags: PF_R|PF_X
		write<std::uint32_t>(0); // align
	}
	void write_headers() {
		write_elf_header();
		write_program_header();
	}
	std::size_t get_position() const {
		return data.size();
	}
	void write_file(const char* path) {
		write<std::uint32_t>(68, data.size()); // filesz
		write<std::uint32_t>(72, data.size()); // memsz
		std::ofstream file(path);
		file.write(data.data(), data.size());
	}
	void MOV(Register dst, Register src) {
		opcode(0x8B);
		operand(dst, src);
	}
	void MOV(Register dst, std::uint32_t value) {
		opcode(0xB8 | dst);
		write<std::uint32_t>(value);
	}
	void MOV(Register dst, Ptr src) {
		opcode(0x8B);
		operand(dst, src);
	}
	void MOV(Ptr dst, Register src) {
		opcode(0x89);
		operand(src, dst);
	}
	void MOV(Ptr dst, std::uint32_t imm) {
		opcode(0xC7);
		operand(0, dst);
		write<std::uint32_t>(imm);
	}
	void MOVZX(Register dst, Register src) {
		opcode_0F(0xB6);
		operand(dst, src);
	}
	void LEA(Register dst, Ptr src) {
		opcode(0x8D);
		operand(dst, src);
	}
	void ADD(Register dst, Register src) {
		opcode(0x03);
		operand(dst, src);
	}
	void ADD(Register dst, std::uint32_t value) {
		opcode(0x81);
		operand(0, dst);
		write<std::uint32_t>(value);
	}
	void SUB(Register dst, Register src) {
		opcode(0x2B);
		operand(dst, src);
	}
	// EDX:EAX = EAX * r
	void IMUL(Register r) {
		opcode(0xF7);
		operand(0x5, r);
	}
	// EAX = EDX:EAX / r
	// EDX = EDX:EAX % r
	void IDIV(Register r) {
		opcode(0xF7);
		operand(0x7, r);
	}
	// EDX:EAX = EAX
	void CDQ() {
		opcode(0x99);
	}
	void PUSH(Register r) {
		opcode(0x50 | r);
	}
	void PUSH(std::uint32_t value) {
		opcode(0x68);
		write<std::uint32_t>(value);
	}
	void POP(Register r) {
		opcode(0x58 | r);
	}
	void CMP(Register r1, Register r2) {
		opcode(0x3B);
		operand(r1, r2);
	}
	void CMP(Register r, std::uint32_t value) {
		opcode(0x81);
		operand(0x7, r);
		write<std::uint32_t>(value);
	}
	void SETE(Register r) {
		opcode_0F(0x94);
		operand(0, r);
	}
	void SETNE(Register r) {
		opcode_0F(0x95);
		operand(0, r);
	}
	void SETL(Register r) {
		opcode_0F(0x9C);
		operand(0, r);
	}
	void SETLE(Register r) {
		opcode_0F(0x9E);
		operand(0, r);
	}
	void SETG(Register r) {
		opcode_0F(0x9F);
		operand(0, r);
	}
	void SETGE(Register r) {
		opcode_0F(0x9D);
		operand(0, r);
	}
	Jump JMP() {
		opcode(0xE9);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(position);
	}
	Jump JE() {
		opcode_0F(0x84);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(position);
	}
	Jump JNE() {
		opcode_0F(0x85);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(position);
	}
	Jump CALL() {
		opcode(0xE8);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(position);
	}
	void RET() {
		opcode(0xC3);
	}
	void INT(std::uint8_t x) {
		opcode(0xCD);
		write<std::uint8_t>(x);
	}
	template <class T> void comment(const T&) {}
};

class TextAssembler {
	Printer printer;
	static StringView print_register(Register r) {
		switch (r) {
		case EAX:
			return "EAX";
		case ECX:
			return "ECX";
		case EDX:
			return "EDX";
		case EBX:
			return "EBX";
		case ESP:
			return "ESP";
		case EBP:
			return "EBP";
		case ESI:
			return "ESI";
		case EDI:
			return "EDI";
		default:
			return StringView();
		}
	}
	class PrintPtr {
		Ptr ptr;
	public:
		constexpr PrintPtr(Ptr ptr): ptr(ptr) {}
		void print(const Printer& p) const {
			p.print(format("[% + %]", print_register(ptr.get_register()), print_number(ptr.get_offset())));
		}
	};
	static PrintPtr print_ptr(Ptr ptr) {
		return PrintPtr(ptr);
	}
public:
	class Jump {
	public:
		void set_target(TextAssembler& assembler, std::size_t target) const {}
	};
	void write_headers() {}
	std::size_t get_position() const {
		return 0;
	}
	void write_file(const char*) {}
	void MOV(Register dst, Register src) {
		printer.println(format("  MOV %, %", print_register(dst), print_register(src)));
	}
	void MOV(Register dst, std::uint32_t value) {
		printer.println(format("  MOV %, %", print_register(dst), print_number(value)));
	}
	void MOV(Register dst, Ptr src) {
		printer.println(format("  MOV %, %", print_register(dst), print_ptr(src)));
	}
	void MOV(Ptr dst, Register src) {
		printer.println(format("  MOV %, %", print_ptr(dst), print_register(src)));
	}
	void MOV(Ptr dst, std::uint32_t imm) {
		printer.println(format("  MOV %, %", print_ptr(dst), print_number(imm)));
	}
	void MOVZX(Register dst, Register src) {
		printer.println(format("  MOVZX %, %", print_register(dst), print_register(src)));
	}
	void LEA(Register dst, Ptr src) {
		printer.println(format("  LEA %, %", print_register(dst), print_ptr(src)));
	}
	void ADD(Register dst, Register src) {
		printer.println(format("  ADD %, %", print_register(dst), print_register(src)));
	}
	void ADD(Register dst, std::uint32_t value) {
		printer.println(format("  ADD %, %", print_register(dst), print_number(value)));
	}
	void SUB(Register dst, Register src) {
		printer.println(format("  SUB %, %", print_register(dst), print_register(src)));
	}
	// EDX:EAX = EAX * r
	void IMUL(Register r) {
		printer.println(format("  IMUL %", print_register(r)));
	}
	// EAX = EDX:EAX / r
	// EDX = EDX:EAX % r
	void IDIV(Register r) {
		printer.println(format("  IDIV %", print_register(r)));
	}
	// EDX:EAX = EAX
	void CDQ() {
		printer.println("  CDQ");
	}
	void PUSH(Register r) {
		printer.println(format("  PUSH %", print_register(r)));
	}
	void PUSH(std::uint32_t value) {
		printer.println(format("  PUSH %", print_number(value)));
	}
	void POP(Register r) {
		printer.println(format("  POP %", print_register(r)));
	}
	void CMP(Register r1, Register r2) {
		printer.println(format("  CMP %, %", print_register(r1), print_register(r2)));
	}
	void CMP(Register r, std::uint32_t value) {
		printer.println(format("  CMP %, %", print_register(r), print_number(value)));
	}
	void SETE(Register r) {
		printer.println(format("  SETE %", print_register(r)));
	}
	void SETNE(Register r) {
		printer.println(format("  SETNE %", print_register(r)));
	}
	void SETL(Register r) {
		printer.println(format("  SETL %", print_register(r)));
	}
	void SETLE(Register r) {
		printer.println(format("  SETLE %", print_register(r)));
	}
	void SETG(Register r) {
		printer.println(format("  SETG %", print_register(r)));
	}
	void SETGE(Register r) {
		printer.println(format("  SETGE %", print_register(r)));
	}
	Jump JMP() {
		printer.println("  JMP");
		return Jump();
	}
	Jump JE() {
		printer.println("  JE");
		return Jump();
	}
	Jump JNE() {
		printer.println("  JNE");
		return Jump();
	}
	Jump CALL() {
		printer.println("  CALL");
		return Jump();
	}
	void RET() {
		printer.println("  RET");
	}
	void INT(std::uint8_t x) {
		printer.println(format("  INT %", print_number(x)));
	}
	template <class T> void comment(const T& t) {
		printer.println(format("  ; %", t));
	}
};
