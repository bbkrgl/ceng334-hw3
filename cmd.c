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
	uint32_t dir_cluster = find_dir_cluster(dir, 1);

	file_entry* fe = 0;
	int dot = 1;
	int dirs_read = read_directory_table(dir_cluster, &fe);
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

void concat_cwd(char* newcwd)
{
	char* nwd = strdup(newcwd);
	char* curr = strsep(&nwd, "/");

	while (curr != NULL) {
		if (strlen(curr) == 0)
			continue;
		if (!strcmp(curr, "..")) {
			char* sign = strrchr(CWD, '/');
			*sign = '\0';
		} else {
			if (CWD[strlen(CWD) - 1] != '/')
				strcat(CWD, "/");
			strcat(CWD, curr);
		}

		strcat(CWD, "/");
		curr = strsep(&nwd, "/");
	}

	if (strlen(CWD) > 1)
		CWD[strlen(CWD) - 1] = '\0';

	free(nwd);
}

void cd(char* newcwd)
{
	int newcwd_cluster = find_dir_cluster(newcwd, 1);
	if (newcwd_cluster) {
		if (newcwd[0] == '/') {
			free(CWD);
			CWD = strdup(newcwd);
		} else {
			concat_cwd(newcwd);
		}
		CWD_cluster = newcwd_cluster;
	}
}

void cat(char* file)
{
	int file_cluster = find_dir_cluster(file, 0);
	if (!file_cluster || file_cluster == bpb.extended.RootCluster)
		return;

	char* buffer = 0;
	int clusters_read = read_clusters(0, file_cluster, (void**) &buffer, -1);
	int chars_per_cluster = bpb.SectorsPerCluster * BPS;
	int i = 0;
	for (; i < clusters_read * chars_per_cluster; i++) {
		if (buffer[i] == 0)
			break;
		printf("%c", buffer[i]);
	}
	if (buffer[i - 1] != '\n')
		printf("\n");
}