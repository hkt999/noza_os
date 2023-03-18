#pragma once

#include <stdint.h>

#define STREQ(s1, s2)	(strcmp(s1, s2) == 0)
//#define DBG(...)		printf("ELF: " __VA_ARGS__)
#define DBG(...)

typedef struct {
	const char	*name; 
	void 		*ptr;
} elf_symbol_t;

typedef struct {
	const elf_symbol_t 	*exported;
	uint32_t 			exported_size; // TODO: use declaration size (uint32_t)
} elf_env_t;

typedef struct {
	void 	*data;
	int		sec_idx;
	int		rel_sec_idx;
	int		need_free;
} elf_sec_t;

typedef struct {
	// LEF binary data
	uint8_t 	*elfbin;
	uint32_t	binsize;

	// exported symbols from dynamic library
	elf_symbol_t *exported_symbols;
	uint32_t	sections;
	int			section_table;
	int			section_table_strings;

	uint32_t	symbol_count;
	int			symbol_table;
	int			symbol_table_strings;
	int			entry_offset;

	elf_sec_t	text;
	elf_sec_t	rodata;
	elf_sec_t	data;
	elf_sec_t	bss;
	const elf_env_t *env;
} elf_obj_t;

int elf_load(elf_obj_t *elf_obj, uint8_t *elfbin, uint32_t elf_bin_size, const elf_env_t *env);
void *elf_bind(elf_obj_t *elf_obj, const char *name);
void elf_free(elf_obj_t *elf_obj);
