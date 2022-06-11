#include "cmd.h"

void print_name(void* str, int start, int len)
{
	for (int i = start; i < len; i++) {
		uint16_t c = ((uint16_t*) str)[i];
		if (c == 0xFFFF)
			continue;
		printf("%c", c);
	}
}

void ls(char* dir, int pp)
{
	uint32_t dir_cluster = find_dir_cluster(dir);

	file_entry* fe = 0;
	int dot = 1;
	int dirs_read = read_directory_entry(dir_cluster, &fe, 1);
	if (!pp) {
		for (int i = 0; i < dirs_read; i++) {
			if (fe[i].lfnc == 0) {
				if (fe[i].msdos.filename[0] == 0x2E) {
					printf(".");
					if (dot)
						printf(".");
					dot--;
				} else {
					print_name(fe[i].msdos.filename, 1, 8);
					print_name(fe[i].msdos.extension, 0, 3);
				}
			} else {
				for (int j = fe[i].lfnc - 1; j >= 0; j--) {
					print_name(fe[i].lfn_list[j].name1, 0, 5);
					print_name(fe[i].lfn_list[j].name2, 0, 6);
					print_name(fe[i].lfn_list[j].name3, 0, 2);
				}
			}
			printf(" ");
		}
		printf("\n");
	} else {
		for (int i = 0; i < dirs_read; i++) {
			if (fe[i].msdos.attributes & 0x10) { // FIXME: Time is not correct
				printf("drwx------ 1 root root 0 %d %s %d %d:%d ",
					fe[i].msdos.fileSize, months[((fe[i].msdos.modifiedDate >> 5) & 0x0F) - 1],
					fe[i].msdos.modifiedDate & 0x1F, fe[i].msdos.modifiedTime >> 11,
	   				(fe[i].msdos.modifiedTime >> 5) & 0x3F);
			} else {
				printf("-rwx------ 1 root root %d %s %d %d:%d ",
					fe[i].msdos.fileSize, months[((fe[i].msdos.modifiedDate >> 5) & 0x0F) - 1],
					fe[i].msdos.modifiedDate & 0x1F, fe[i].msdos.modifiedTime >> 11,
	   				(fe[i].msdos.modifiedDate >> 5) & 0x3F);
			}

			if (fe[i].lfnc == 0) {
				if (fe[i].msdos.filename[0] == 0x2E) {
					printf(".");
					if (dot)
						printf(".");
					dot = 1;
				} else {
					print_name(fe[i].msdos.filename, 1, 8);
					print_name(fe[i].msdos.extension, 0, 3);
				}
			} else {
				for (int j = fe[i].lfnc - 1; j >= 0; j--) {
					print_name(fe[i].lfn_list[j].name1, 0, 5);
					print_name(fe[i].lfn_list[j].name2, 0, 6);
					print_name(fe[i].lfn_list[j].name3, 0, 2);
				}
			}
			printf("\n");
		}	
	}
}

void cd(char* newcwd)
{
	int newcwd_cluster = find_dir_cluster(newcwd);
	if (newcwd_cluster) {
		if (CWD[strlen(CWD) - 1] != '/')
			CWD = strcat(CWD, "/");
		CWD = strcat(CWD, newcwd);
		CWD_cluster = newcwd_cluster;
	}
}
