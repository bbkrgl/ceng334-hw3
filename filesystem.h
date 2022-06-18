#ifndef __HW3_FS__
#define __HW3_FS__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

#include "fat32.h"

#pragma pack(push, 1)
typedef struct FAT_entry {
	unsigned int address: 28;
	unsigned int mask: 4;
} FAT_entry;
#pragma pack(pop)

typedef struct file_entry {
	int lfnc;
	FatFileLFN* lfn_list;
	FatFile83 msdos;
} file_entry;

extern int fs_fd;
extern BPB_struct bpb;
extern FAT_entry** fat_table;
extern uint32_t CWD_cluster;
extern char* CWD;

extern char* months[];

void open_fs(char* fsname);
int read_clusters(int fat_id, uint32_t cluster_num, void** data, int size);
int read_directory_table(int cluster_num, file_entry** directory);
uint32_t find_dir_cluster(char* dir, file_entry **fe_return_parent,
	int* dir_i_return, int* dirs_read_return, int is_dir);
void write_file_entry(char* dir, file_entry* fe, int create_dir);
void write_directory(uint32_t dir_cluster, file_entry* fe_dir, int dir_size);

#endif
