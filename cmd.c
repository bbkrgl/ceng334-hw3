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
			if (fe[i].msdos.attributes & 0x10) {
				printf("drwx------ 1 root root 0 0 %s %d %.2d:%.2d ",
					months[((fe[i].msdos.modifiedDate >> 5) & 0x0F) - 1],
					fe[i].msdos.modifiedDate & 0x1F, fe[i].msdos.modifiedTime >> 11,
	   				(fe[i].msdos.modifiedTime >> 5) & 0x3F);
			} else {
				printf("-rwx------ 1 root root %d %s %d %.2d:%.2d ",
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
		if (strlen(curr) == 0) {
			curr = strsep(&nwd, "/");
			continue;
		}
		if (!strcmp(curr, "..")) {
			char* sign = strrchr(CWD, '/');
			*sign = '\0';
		} else {
			if (CWD[strlen(CWD) - 1] != '/')
				strcat(CWD, "/");
			strcat(CWD, curr);
		}

		curr = strsep(&nwd, "/");
	}

	if (strlen(CWD) == 0)
		strcat(CWD, "/");

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

uint8_t checksum_filename(uint8_t* filename)
{
	uint8_t sum = 0;
	for (int i = 11; i >= 0; i--)
		sum = ((sum & 1) << 7) + (sum >> 1) + *filename++;
	return sum;
}

void touch(char* file)
{
	char* dir = strdup(file);
	char* filename = strrchr(dir, '/');
	if (filename != NULL) {
		*filename = 0; 
		filename++;
	} else {
		filename = dir;
		dir = strdup(CWD);
	}

	if (filename == NULL)
		return;

	file_entry fe;
	for (int i = 0; i < 8; i++) {
		if (i < strlen(filename))
			if (filename[i] == 0xE5)
				fe.msdos.filename[i] = 0x05;
			else
				fe.msdos.filename[i] = filename[i];
		else
			fe.msdos.filename[i] = 0xFF;
	}
	char* extension = 0;
	if (strrchr(filename, '.') != NULL) {
		extension = strrchr(filename, '.');
		*extension = 0, extension++;
	}

	for (int i = 0; i < 3; i++) {
		if (extension && i < strlen(extension))
			fe.msdos.extension[i] = extension[i];
		else
			fe.msdos.extension[i] = ' ';
	}
	fe.msdos.attributes = 0x20;
	fe.msdos.reserved = 0;

	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime); // or gmtime()
	fe.msdos.creationDate = fe.msdos.modifiedDate =
		((timeinfo->tm_year - 80) << 9) | ((timeinfo->tm_mon << 5) + 1) | (timeinfo->tm_mday);
	fe.msdos.creationTime = fe.msdos.modifiedTime =
		((timeinfo->tm_hour << 11) | (timeinfo->tm_min << 5) | (timeinfo->tm_sec >> 1));

	fe.msdos.eaIndex = 0;
	fe.msdos.firstCluster = 0;
	fe.msdos.fileSize = 0;

	uint8_t checksum = checksum_filename(fe.msdos.extension);

	fe.lfn_list = 0;
	fe.lfnc = strlen(filename) / 13 + 1;
	char c = ' ';
	int seq_num = 1;
	for (int i = fe.lfnc - 1; i >= 0; i--) {
		fe.lfn_list = realloc(fe.lfn_list, sizeof(FatFileLFN) * (i + 1));
		fe.lfn_list[i].sequence_number = seq_num, seq_num++;
		fe.lfn_list[i].attributes = 0x0F;
		fe.lfn_list[i].firstCluster = 0x00;
		fe.lfn_list[i].checksum = checksum;
		fe.lfn_list[i].reserved = 0;
		for (int j = 0; j < 5; j++) {
			if (strlen(filename) < j + i * 13)
				fe.lfn_list[i].name1[j] = 0xFFFF;
			else
				fe.lfn_list[i].name1[j] = filename[j + i * 13];
		}

		for (int j = 0; j < 6; j++) {
			if (strlen(filename) < j + 5 + i * 13)
				fe.lfn_list[i].name2[j] = 0xFFFF;
			else
				fe.lfn_list[i].name2[j] = filename[j + 5 + i * 13];
		}

		for (int j = 0; j < 2; j++) {
			if (strlen(filename) < j + 11 + i * 13)
				fe.lfn_list[i].name3[j] = 0xFFFF;
			else
				fe.lfn_list[i].name3[j] = filename[j + 11 + i * 13];
		}
	}
	fe.lfn_list[0].sequence_number = 'A';

	write_file_entry(dir, &fe);
	free(dir);
}
