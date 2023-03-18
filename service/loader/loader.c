#include <string.h>
#include <stdlib.h>
#include "loader.h"
#include "elf.h"
#include "app/sysent.h"

#define IS_FLAGS_SET(v, m) ((v&m) == m)
#define SECTION_OFFSET(e, n) (e->section_table + n * sizeof(elf32_shdr))

typedef enum {
	FOUND_ERROR = 0,
	FOUND_SYM_TAB = (1 << 0),
	FOUND_STR_TAB = (1 << 2),
	FOUND_TEXT = (1 << 3),
	FOUND_RO_DATA = (1 << 4),
	FOUND_DATA = (1 << 5),
	FOUND_BSS = (1 << 6),
	FOUND_REL_TEXT = (1 << 7),
	FOUND_REL_RO_DATA = (1 << 8),
	FOUND_REL_DATA = (1 << 9),
	FOUND_REL_BSS = (1 << 10),
	FOUND_VALID = FOUND_SYM_TAB | FOUND_STR_TAB,
	FOUND_EXEC = FOUND_VALID | FOUND_TEXT,
	FOUND_ALL = FOUND_SYM_TAB | FOUND_STR_TAB | FOUND_TEXT | FOUND_RO_DATA | FOUND_DATA
	           | FOUND_BSS | FOUND_REL_TEXT | FOUND_REL_RO_DATA | FOUND_REL_DATA | FOUND_REL_BSS
} find_flags_t;

static int read_section_name(elf_obj_t *e, int off, char **buf)
{
	*buf = (char *)(e->elfbin + e->section_table_strings + off);
	return 0;
}

static int read_symbol_name(elf_obj_t *e, int off, char **buf)
{
	*buf = (char *)(e->elfbin + e->symbol_table_strings + off);
	return 0;
}

#if 0
static uint32_t swabo(uint32_t hl)
{
	return ((((hl) >> 24)) | /* */
	        (((hl) >> 8) & 0x0000ff00) | /* */
	        (((hl) << 8) & 0x00ff0000) | /* */
	        (((hl) << 24))); /* */
}
#endif

static void dump_data(uint8_t *data, uint32_t size)
{
#if 1
	int i = 0;
	while (i < size) {
		if ((i & 0xf) == 0)
			DBG("\n  %04X: ", i);
		DBG("%08x ", swabo(*((uint32_t*)(data + i))));
		i += sizeof(uint32_t);
	}
	DBG("\n");
#endif
}

static int load_section_data(elf_obj_t *e, elf_sec_t *s, elf32_shdr *h, int need_allocate)
{
	if (!h->sh_size) {
		DBG(" no data for section\n");
		return 0;
	}
	if (need_allocate) {
		s->data = (uint8_t *)malloc(h->sh_size); // need to be 4 bytes alignment
	} else {
		s->data = (uint8_t *)e->elfbin + h->sh_offset;
	}
	s->need_free = need_allocate;

	DBG("DATA: ");
	dump_data(s->data, h->sh_size);

	return 0;
}

static int read_section_header(elf_obj_t *e, int n, elf32_shdr **h) // TODO: change to pointer interation
{
	*h = (elf32_shdr *)(e->elfbin + SECTION_OFFSET(e, n));
	return 0;
}

static int read_section(elf_obj_t *e, int n, elf32_shdr **h, char **name)
{
	if (read_section_header(e, n, h) != 0)
		return -1;

	if ((*h)->sh_name)
		return read_section_name(e, (*h)->sh_name, name);

	return 0;
}

static int read_symbol(elf_obj_t *e, int n, elf32_sym **sym, char **name)
{
	int ret = -1;
	*sym = (elf32_sym *)(e->elfbin + e->symbol_table + n * sizeof(elf32_sym));
	if ((*sym)->st_name)
		ret = read_symbol_name(e, (*sym)->st_name, name);
	else {
		elf32_shdr *shdr;
		ret = read_section(e, (*sym)->st_shndx, &shdr, name);
	}
	return ret;
}

#if 0
static const char *type_str(int symt)
{
#define STRCASE(name) case name: return #name;
	switch (symt) {
		STRCASE(R_ARM_NONE)
		STRCASE(R_ARM_ABS32)
		STRCASE(R_ARM_THM_PC22)
		STRCASE(R_ARM_THM_JUMP24)
	default:
		return "R_<unknow>";
	}
#undef STRCASE
}
#endif

static void rel_jump_call(uint32_t rel_addr, int type, uint32_t sym_addr)
{
	uint16_t upper_insn = ((uint16_t *) rel_addr)[0];
	uint16_t lower_insn = ((uint16_t *) rel_addr)[1];
	uint32_t S = (upper_insn >> 10) & 1;
	uint32_t J1 = (lower_insn >> 13) & 1;
	uint32_t J2 = (lower_insn >> 11) & 1;

	int32_t offset = (S << 24) | /* S     -> offset[24] */
	                 ((~(J1 ^ S) & 1) << 23) | /* J1    -> offset[23] */
	                 ((~(J2 ^ S) & 1) << 22) | /* J2    -> offset[22] */
	                 ((upper_insn & 0x03ff) << 12) | /* imm10 -> offset[12:21] */
	                 ((lower_insn & 0x07ff) << 1); /* imm11 -> offset[1:11] */
	if (offset & 0x01000000)
		offset -= 0x02000000;

	offset += sym_addr - rel_addr;

	S = (offset >> 24) & 1;
	J1 = S ^ (~(offset >> 23) & 1);
	J2 = S ^ (~(offset >> 22) & 1);

	upper_insn = ((upper_insn & 0xf800) | (S << 10) | ((offset >> 12) & 0x03ff));
	((uint16_t *) rel_addr)[0] = upper_insn;

	lower_insn = ((lower_insn & 0xd000) | (J1 << 13) | (J2 << 11)
	              | ((offset >> 1) & 0x07ff));
	((uint16_t *) rel_addr)[1] = lower_insn;
}

static int relocate_symbol(uint32_t rel_addr, int type, uint32_t sym_addr)
{
	DBG("relocate symbol ~~~~ [%s] sym_addr:0x%08X, rel_addr:0x%08X\n", type_str(type), sym_addr, rel_addr);
	switch (type) {
		case R_ARM_ABS32:
			DBG("  -- relocated address: 0x%08X\n", *((uint32_t *)rel_addr));
			DBG("  R_ARM_ABS32 relocated is 0x%08X (before)\n", *((uint32_t *)rel_addr));
			*((uint32_t *)rel_addr) += sym_addr;
			DBG("  R_ARM_ABS32 relocated is 0x%08X (after)\n", *((uint32_t *)rel_addr));
			break;

		case R_ARM_THM_PC22:
		case R_ARM_THM_JUMP24:
			DBG("  R_ARM_THM_CALL/JMP relocated is 0x%08X\n", *((uint32_t *)rel_addr));
			rel_jump_call(rel_addr, type, sym_addr);
			break;

		default:
			DBG("  undefined relocation %d\n", type);
			return -1;
	}
	return 0;
}

static elf_sec_t *section_of(elf_obj_t *e, int index)
{
#define IFSECTION(sec, i) \
	do { \
		if ((sec).sec_idx == i) \
			return &(sec); \
	} while(0)
	IFSECTION(e->text, index);
	IFSECTION(e->rodata, index);
	IFSECTION(e->data, index);
	IFSECTION(e->bss, index);
#undef IFSECTION
	return NULL;
}

static uint32_t address_of(elf_obj_t *e, elf32_sym *sym, const char *name)
{
	if (sym->st_shndx == SHN_UNDEF) {
		int i;
		for (i = 0; i < e->env->exported_size; i++)
			if (STREQ(e->env->exported[i].name, name))
				return (uint32_t)(e->env->exported[i].ptr);
	} else {
		elf_sec_t *sym_section = section_of(e, sym->st_shndx);
		if (sym_section)
			return ((uint32_t) sym_section->data) + sym->st_value;
	}
	DBG("  can not find address for symbol %s\n", name);
	return 0xffffffff;
}

static int relocate(elf_obj_t *e, elf32_shdr *h, elf_sec_t *s, const char *name)
{
	if (s->data) {
		elf32_rel *rel;
		uint32_t rel_entries = h->sh_size / sizeof(elf32_rel);
		uint32_t rel_count;

		DBG(" offset   Info     Type             Name\n");
		rel = (elf32_rel *) (e->elfbin + h->sh_offset);
		for (rel_count = 0; rel_count < rel_entries; rel_count++) {
			elf32_sym *sym;
			uint32_t sym_addr;
			char *name = NULL;
			int sym_entry = ELF32_R_SYM(rel->r_info);
			int rel_type = ELF32_R_TYPE(rel->r_info);
			uint32_t rel_addr = ((uint32_t) s->data) + rel->r_offset;
			read_symbol(e, sym_entry, &sym, &name);
			DBG("-------------------name: %s (%d/%d)\n", name, rel_count, rel_entries);
			DBG(" %08X %08X %-16s %s\n", (unsigned int) rel->r_offset, (unsigned int) rel->r_info, type_str(rel_type), name);

			sym_addr = address_of(e, sym, name);
			if (sym_addr != 0xffffffff) {
				//DBG("  sym_addr=%08X rel_addr=%08X\n", (unsigned int) sym_addr, (unsigned int) rel_addr);
				if (relocate_symbol(rel_addr, rel_type, sym_addr) == -1)
					return -1;
			} else {
				DBG("  no symbol address of %s\n", name);
				return -1;
			}
			rel++; // check next relocation section
		}
		return 0;
	} else {
		DBG("section not loaded");
	}
	return -1;
}

int place_info(elf_obj_t *e, elf32_shdr *sh, const char *name, int n)
{
	if (STREQ(name, ".symtab")) {
		e->symbol_table = sh->sh_offset;
		e->symbol_count = sh->sh_size / sizeof(elf32_sym);
		return FOUND_SYM_TAB;
	} else if (STREQ(name, ".strtab")) {
		e->symbol_table_strings = sh->sh_offset;
		return FOUND_STR_TAB;
	} else if (STREQ(name, ".text")) {
		if (load_section_data(e, &e->text, sh, 0) == -1)
			return FOUND_ERROR;
		e->text.sec_idx = n;
		return FOUND_TEXT;
	} else if (STREQ(name, ".rodata")) {
		if (load_section_data(e, &e->rodata, sh, 0) == -1)
			return FOUND_ERROR;
		e->rodata.sec_idx = n;
		return FOUND_RO_DATA;
	} else if (STREQ(name, ".data")) {
		if (load_section_data(e, &e->data, sh, 0) == -1)
			return FOUND_ERROR;
		e->data.sec_idx = n;
		return FOUND_DATA;
	} else if (STREQ(name, ".bss")) {
		if (load_section_data(e, &e->bss, sh, 1) == -1)
			return FOUND_ERROR;
		e->bss.sec_idx = n;
		return FOUND_BSS;
	} else if (STREQ(name, ".rel.text")) {
		e->text.rel_sec_idx = n;
		return FOUND_REL_TEXT;
	} else if (STREQ(name, ".rel.rodata")) {
		e->rodata.rel_sec_idx = n;
		return FOUND_REL_TEXT;
	} else if (STREQ(name, ".rel.data")) {
		e->data.rel_sec_idx = n;
		return FOUND_REL_TEXT;
	}

	return 0;
}

static int load_symbols(elf_obj_t *e)
{
	int n;
	int founded = 0;
	for (n = 1; n < e->sections; n++) {
		elf32_shdr *sect_header;
		char *name = NULL;
		if (read_section_header(e, n, &sect_header) != 0) {
			DBG("error reading section");
			return -1;
		}
		if (sect_header->sh_name)
			read_section_name(e, sect_header->sh_name, &name);

		DBG("examining section %d %s, size=%d\n", n, name, sect_header->sh_size);
		founded |= place_info(e, sect_header, name, n);
		if (IS_FLAGS_SET(founded, FOUND_ALL))
			return FOUND_ALL;
	}
	DBG("done\n");
	return founded;
}

static int elf_init(elf_obj_t *e)
{
	elf32_ehdr *h;
	elf32_shdr *sH;

	h = (elf32_ehdr *)(e->elfbin);
	sH = (elf32_shdr *)(e->elfbin + h->e_shoff + h->e_shstrndx * sizeof(elf32_shdr));

	e->entry_offset = h->e_entry;
	e->sections = h->e_shnum;
	e->section_table = h->e_shoff;
	e->section_table_strings = sH->sh_offset;

	return 0;
}

static void free_section(elf_sec_t *s)
{
	if (s->need_free && s->data) {
		free(s->data);
	}
}

static int relocate_section(elf_obj_t *e, elf_sec_t *s, const char *name)
{
	DBG("----- relocating section %s\n", name);
	if (s->rel_sec_idx) {
		elf32_shdr *sect_header;
		if (read_section_header(e, s->rel_sec_idx, &sect_header) == 0) {
			return relocate(e, sect_header, s, name);
		} else {
			DBG("error reading section header\n");
			return -1;
		}
	} else {
		DBG("no relocation index\n"); /* Not an error */
	}
	return 0;
}

static int relocate_sections(elf_obj_t *e)
{
	return relocate_section(e, &e->text, ".text")
	       | relocate_section(e, &e->rodata, ".rodata")
	       | relocate_section(e, &e->data, ".data");
}

// exported API
int elf_load(elf_obj_t *e, uint8_t *elfbin, uint32_t elf_bin_size, const elf_env_t *env) 
{
	DBG("load address: %p, size: %d\n", elfbin, elf_bin_size);
	memset(e, 0, sizeof(elf_obj_t));
	e->elfbin = elfbin;
	e->binsize = elf_bin_size;
	if (elf_init(e) != 0) {
		DBG("invalid ELF object -1\n");
		return -1;
	}
	e->env = env;
	if (IS_FLAGS_SET(load_symbols(e), FOUND_VALID)) {
		int ret = -1;
		if (relocate_sections(e) == 0) {
			if (e->entry_offset) {
				entry_t *entry_func = (entry_t*)(e->text.data + e->entry_offset);
				DBG("jump entry ! (%p)\n", entry_func);
				entry_func((void *)&e->exported_symbols);
			} else {
				DBG("no entry defined.");
				return -1;
			}
			return 0;
		}
		return ret;
	} else {
		DBG("invalid ELF object -2\n");
		return -2;
	}
}

void elf_free(elf_obj_t *e)
{
	// only need to free .bss section
	free_section(&e->bss);
}

void *elf_bind(elf_obj_t *e, const char *name)
{
	if (e->exported_symbols) {
		elf_symbol_t *p = e->exported_symbols;
		while (p->name) {
			DBG("symbol: %s, addr: %p\n", p->name, p->ptr);
			if (STREQ(p->name, name)) {
				return p->ptr;
			}
			p++;
		}
	}

	return NULL;
}

