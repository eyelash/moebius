#pragma once

#include <cstdint>
#include <initializer_list>
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

static constexpr std::uint64_t VADDR = 0x10000;
using Addr = std::uint32_t;
static constexpr Addr ELF_HEADER_SIZE = 52;
static constexpr Addr PROGRAM_HEADER_SIZE = 32;

class Assembler {
	std::vector<std::uint8_t> data;
	template <class T> void write(const T& t) {
		const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&t);
		data.insert(data.end(), ptr, ptr + sizeof(T));
	}
	template <class T> void write(std::initializer_list<T> ts) {
		for (const T& t: ts) {
			write<T>(t);
		}
	}
	template <class T> void write(std::size_t position, const T& t) {
		const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&t);
		std::copy(ptr, ptr + sizeof(T), data.begin() + position);
	}
	void write_elf_header(std::size_t entry_point = 0) {
		write<std::uint8_t>({0x7f, 'E', 'L', 'F'});
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
		write<Addr>(VADDR + ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE + entry_point); // entry point
		write<Addr>(ELF_HEADER_SIZE); // program header
		write<Addr>(0); // section header
		write<std::uint32_t>(0); // flags
		write<std::uint16_t>(ELF_HEADER_SIZE); // ehsize
		write<std::uint16_t>(PROGRAM_HEADER_SIZE); // program header entry size
		write<std::uint16_t>(1); // e_phnum
		write<std::uint16_t>(0);
		write<std::uint16_t>(0);
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
public:
	class Jump {
		Assembler* assembler;
		std::size_t position;
	public:
		Jump(Assembler* assembler, std::size_t position): assembler(assembler), position(position) {}
		void set_target(std::size_t target) const {
			assembler->write<std::uint32_t>(position, target - (position + 4));
		}
	};
	Assembler() {
		write_elf_header();
		write_program_header();
	}
	std::size_t get_position() const {
		return data.size();
	}
	void write_file(const char* path) {
		write<std::uint32_t>(68, ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE + data.size()); // filesz
		write<std::uint32_t>(72, ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE + data.size()); // memsz
		std::ofstream file(path);
		std::copy(data.begin(), data.end(), std::ostreambuf_iterator<char>(file));
	}
	void MOV(Register r, std::uint32_t value) {
		write<std::uint8_t>(0xB8 | r);
		write<std::uint32_t>(value);
	}
	void MOV(Register dst, Register src) {
		write<std::uint8_t>(0x8B);
		write<std::uint8_t>(0xC0 | dst << 3 | src);
	}
	void MOV(Register dst, Ptr src) {
		write<std::uint8_t>(0x8B);
		write<std::uint8_t>(0x80 | dst << 3 | src.get_register());
		if (src.get_register() == ESP) {
			write<std::uint8_t>(0x24);
		}
		write<std::uint32_t>(src.get_offset());
	}
	void MOV(Ptr dst, Register src) {
		write<std::uint8_t>(0x89);
		write<std::uint8_t>(0x80 | src << 3 | dst.get_register());
		if (dst.get_register() == ESP) {
			write<std::uint8_t>(0x24);
		}
		write<std::uint32_t>(dst.get_offset());
	}
	void ADD(Register dst, Register src) {
		write<std::uint8_t>(0x03);
		write<std::uint8_t>(0xC0 | dst << 3 | src);
	}
	void ADD(Register dst, std::uint32_t value) {
		write<std::uint8_t>(0x81);
		write<std::uint8_t>(0xC0 | 0x0 << 3 | dst);
		write<std::uint32_t>(value);
	}
	void SUB(Register dst, Register src) {
		write<std::uint8_t>(0x2B);
		write<std::uint8_t>(0xC0 | dst << 3 | src);
	}
	// EDX:EAX = EAX * r
	void IMUL(Register r) {
		write<std::uint8_t>(0xF7);
		write<std::uint8_t>(0xC0 | 0x5 << 3 | r);
	}
	// EAX = EDX:EAX / r
	// EDX = EDX:EAX % r
	void IDIV(Register r) {
		write<std::uint8_t>(0xF7);
		write<std::uint8_t>(0xC0 | 0x7 << 3 | r);
	}
	void CDQ() {
		write<std::uint8_t>(0x99);
	}
	void PUSH(Register r) {
		write<std::uint8_t>(0x50 | r);
	}
	void PUSH(std::uint32_t value) {
		write<std::uint8_t>(0x68);
		write<std::uint32_t>(value);
	}
	void POP(Register r) {
		write<std::uint8_t>(0x58 | r);
	}
	void CMP(Register r1, Register r2) {
		write<std::uint8_t>(0x3B);
		write<std::uint8_t>(0xC0 | r1 << 3 | r2);
	}
	void SETL(Register r) {
		write<std::uint8_t>({0x0F, 0x9C});
		write<std::uint8_t>(0xC0 | r);
	}
	Jump JMP() {
		write<std::uint8_t>(0xE9);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(this, position);
	}
	Jump JNE() {
		write<std::uint8_t>(0x0F);
		write<std::uint8_t>(0x85);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(this, position);
	}
	Jump CALL() {
		write<std::uint8_t>(0xE8);
		const std::size_t position = data.size();
		write<std::uint32_t>(0);
		return Jump(this, position);
	}
	void RET() {
		write<std::uint8_t>(0xC3);
	}
	void INT(std::uint8_t x) {
		write<std::uint8_t>(0xCD);
		write<std::uint8_t>(x);
	}
};
