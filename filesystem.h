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

static int fs_fd = 0;
static BPB_struct bpb;
static FAT_entry** fat_table;

void open_fs(char* fsname);

#endif
