#ifndef __HW3_FS__
#define __HW3_FS__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "fat32.h"

#pragma pack(push, 1)
typedef struct FAT_entry {
	unsigned int address: 28;
	unsigned int mask: 4;
} FAT_entry;
#pragma pack(pop)

typedef struct file_entry {
	int lfnc;
	char* lfn_filename;
	FatFileLFN* lfn_list;
	FatFile83 msdos;
} file_entry;

static int fs_fd = 0;
static BPB_struct bpb;
static FAT_entry** fat_table;
static char* CWD = "/";
static uint32_t CWD_cluster;

static char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

void open_fs(char* fsname);
int read_cluster(int fat_id, uint32_t cluster_num, void** data, int size);
int read_directory_entry(int cluster_num, file_entry** directory, int size);
uint32_t find_dir_cluster(char* dir);

#endif
