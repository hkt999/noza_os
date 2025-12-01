#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h> 
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include "printk.h"

typedef struct tree_s tree_t;

typedef struct dir_rec_s {
	char *name;
	tree_t *tree;
} dir_rec_t;

#pragma pack(push)
#pragma pack(1)
struct tree_s {
	uint16_t dir_count;
	dir_rec_t dir[256];
	uint16_t file_count;
	char *file_name[256];
};

typedef struct serial_item_s {
	uint32_t name_offset;
	uint32_t data_offset;
} serial_item_t;

// data structure for read
typedef struct read_dir_s {
	uint16_t dir_count;
	uint16_t file_count;
	serial_item_t item[];
} read_dir_t;

#pragma pack(pop)


size_t size_of_tree(tree_t *tree)
{
	return sizeof(tree->dir_count) + sizeof(tree->file_count) + 
		(tree->dir_count + tree->file_count) * sizeof(serial_item_t);
}

void padding_4bytes(FILE *outfile)
{
	uint32_t pos = (uint32_t) ftell(outfile);
	if (pos % 4 != 0) {
		uint8_t pad = 0;
		fwrite(&pad, 1, 4 - (pos % 4), outfile);
	}
}

void build_name_list(uint32_t table_offset, tree_t *tree, FILE *outfile)
{
	// build name offset (directory)
	for (int i=0; i<tree->dir_count; i++) {
		uint32_t pos = (uint32_t) ftell(outfile);
		// update table
		fseek(outfile, table_offset + sizeof(serial_item_t) * i, SEEK_SET);
		fwrite(&pos, 1, sizeof(pos), outfile);
		// seek back to tail and write string data
		fseek(outfile, pos, SEEK_SET); 
		fwrite(tree->dir[i].name, 1, strlen(tree->dir[i].name) + 1, outfile);
	}

	// build name offset (file)
	for (int i=0; i<tree->file_count; i++) {
		uint32_t pos = (uint32_t ) ftell(outfile);
		// update table
		fseek(outfile, table_offset + sizeof(serial_item_t) * (i + tree->dir_count), SEEK_SET);
		fwrite(&pos, 1, sizeof(pos), outfile);
		// seek back to tail and write string data
		fseek(outfile, pos, SEEK_SET);
		fwrite(tree->file_name[i], 1, strlen(tree->file_name[i]) + 1, outfile);
	}

	padding_4bytes(outfile);
}

void write_file_binary(char *root_dir, char *filename, FILE *outfile)
{
	uint8_t buf[1024];
	char in_name[256];

	sprintf(in_name, "%s/%s", root_dir, filename);
	FILE *infile = fopen(in_name, "rb");
	if (infile == NULL) {
		printk("error: cannot open file for input (%s)\n", in_name);
		exit(1);
	}

	fseek(infile, 0, SEEK_END);
	uint32_t sz = (uint32_t )ftell(infile);
	fseek(infile, 0, SEEK_SET);

	// write file size
	fwrite(&sz, 1, sizeof(sz), outfile);

	// write file binary
	while (!feof(infile)) {
		int read_bytes = fread(buf, 1, sizeof(buf), infile);
		if (read_bytes > 0) {
			fwrite(buf, 1, read_bytes, outfile);
		}
	}
	padding_4bytes(outfile);

	fclose(infile);
}

void build_file(char *root_dir, int table_offset, tree_t *tree, FILE *outfile)
{
	for (int i=0; i<tree->file_count; i++) {
		uint32_t pos = (uint32_t)ftell(outfile);
		// update table for file offset
		fseek(outfile, table_offset +  sizeof(serial_item_t) * (i + tree->dir_count) + sizeof(uint32_t), SEEK_SET);
		fwrite(&pos, 1, sizeof(pos), outfile);
		fseek(outfile, pos, SEEK_SET);
		write_file_binary(root_dir, tree->file_name[i], outfile);
	}
}

void serialize_tree(char *root_dir, tree_t *tree, FILE *outfile);
void build_tree(char *root_dir, int table_offset, tree_t *tree, FILE *outfile)
{
	for (int i=0; i<tree->dir_count; i++) {
		// update table for directory offset
		uint32_t pos = (uint32_t)ftell(outfile);
		fseek(outfile, table_offset + sizeof(serial_item_t) * i + sizeof(uint32_t), SEEK_SET);
		fwrite(&pos, 1, sizeof(pos), outfile);
		fseek(outfile, pos, SEEK_SET);

		char new_dir[256];
		snprintf(new_dir, sizeof(new_dir), "%s/%s",  root_dir, tree->dir[i].name);
		serialize_tree(new_dir, tree->dir[i].tree, outfile);
	}
}

void serialize_tree(char *root_dir, tree_t *tree, FILE *outfile)
{
	fwrite(&tree->dir_count, 1, sizeof(tree->dir_count), outfile);
	fwrite(&tree->file_count, 1, sizeof(tree->file_count), outfile);

	serial_item_t empty_item;
	memset(&empty_item, 0, sizeof(serial_item_t));
	uint32_t table_offset = (uint32_t)ftell(outfile);

	// create empty table
	for (int i=0; i<tree->dir_count + tree->file_count; i++) {
		fwrite(&empty_item, 1, sizeof(empty_item), outfile);
	}

	build_name_list(table_offset, tree, outfile);
	build_file(root_dir, table_offset, tree, outfile);
	build_tree(root_dir, table_offset, tree, outfile);
}

size_t get_file_size(const char *path)
{
	size_t size;

	FILE *infile = fopen(path, "rb");
	if (infile == NULL) {
		printk("error: file not found (%s)\n", path);
		exit(1);
	}
	fseek(infile, 0, SEEK_END);
	size = ftell(infile);
	fclose(infile);

	return size;
}

void create_tree(tree_t *tree, char *dir)
{
    DIR *dp;
    char mypath[512];
    struct dirent *entry;
    struct stat statbuf;

    if((dp = opendir(dir)) == NULL) {
        printk("cannot open directory: %s\n", dir);
        return;
    }
    while((entry = readdir(dp)) != NULL) {
        snprintf(mypath, sizeof(mypath), "%s/%s", dir, entry->d_name);
		//strcpy(mypath, entry->d_name);
        lstat(mypath, &statbuf);
        if(S_ISDIR(statbuf.st_mode)) {
            if(strcmp(".", entry->d_name) == 0 ||
                strcmp("..", entry->d_name) == 0)
                continue;

			tree->dir[tree->dir_count].name = strdup(entry->d_name);
			tree->dir[tree->dir_count].tree = (tree_t *)malloc(sizeof(tree_t));
            create_tree(tree->dir[tree->dir_count].tree, mypath);
			tree->dir_count++;
        } else {
			tree->file_name[tree->file_count++] = strdup(entry->d_name);
        }
    }
    closedir(dp);
}

void print_indent(int indent) 
{
	for (int i=0; i<indent; i++) {
		printk("    ");
	}
}

void serialize(char *root_dir, tree_t *tree, const char *outname)
{
	FILE *outfile = fopen(outname, "wb");
	if (outfile == NULL) {
		printk("error, cannot create file [%s]\n", outname);
		exit(1);
	}

	serialize_tree(root_dir, tree, outfile);
	fclose(outfile);
}

void romfs_validate_dir(uint8_t *p, uint32_t offset, int indent)
{
	read_dir_t *root = (read_dir_t *)(p+offset);
	for (int j=0; j<indent; j++) {
		printk("   ");
	}
	printk("num_dir: %d  ", root->dir_count);
	printk("num_file: %d\n", root->file_count);
	for (int i=0; i < (root->dir_count + root->file_count); i++) {
		for (int j=0; j<indent; j++) {
			printk("   ");
		}
		if (i < root->dir_count) 
			printk("dir ");
		else 
			printk("file ");

		printk("name offset: %d\n", root->item[i].name_offset);
		printk("	-> [%s]\n", (p + root->item[i].name_offset));
		printk("data offset: %d\n", root->item[i].data_offset);
		if (i < root->dir_count) {
			romfs_validate_dir(p, root->item[i].data_offset, indent++);
		}
	}
}

void romfs_validate(uint8_t *p)
{
	romfs_validate_dir(p, 0, 0);
	printk("OK\n");
}

void romfs_validate_file(const char *bin_name)
{
	FILE *infile = fopen(bin_name, "rb");
	if (infile == NULL) {
		printk("error, cannot open file [%s]\n", bin_name);
		exit(1);
	}
	fseek(infile, 0, SEEK_END);
	size_t size = ftell(infile);
	fseek(infile, 0, SEEK_SET);
	uint8_t *p = (uint8_t *)malloc(size);
	if (p == NULL) {
		printk("error: cannot allocate memory (size=%zu)\n", size);
		exit(1);
	}
	if (fread(p, 1, size, infile) != size) {
		printk("error: read [%s]\n", bin_name);
		exit(1);
	}
	fclose(infile);

	romfs_validate(p);
	printk("Validated !\n");

}

int main(int argc, char **argv)
{
	if (argc < 2) {
		printk("usage: %s [directory name]\n", argv[0]);
		exit(1);
	}

	tree_t tree;
	memset(&tree, 0, sizeof(tree_t));
	create_tree(&tree, argv[1]);

	serialize(argv[1], &tree, "output.bin");
	romfs_validate_file("output.bin");
}
