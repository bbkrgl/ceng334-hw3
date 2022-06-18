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
	uint32_t dir_cluster = find_dir_cluster(dir, 0, 0, 0, 1);

	file_entry* fe = 0;
	int dirs_read = read_directory_table(dir_cluster, &fe);
	if (!pp) {
		for (int i = 0; i < dirs_read; i++) {
			if (fe[i].lfnc == 0) {
				if (fe[i].msdos.filename[0] == 0x2E) {
					printf(".");
					if (fe[i].msdos.filename[1] != ' ')
						printf(".");
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
					if (fe[i].msdos.filename[1] != ' ')
						printf(".");
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
	int newcwd_cluster = find_dir_cluster(newcwd, 0, 0, 0, 1);
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
	uint32_t file_cluster = find_dir_cluster(file, 0, 0, 0, 0);
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
	for (int i = 11; i; i--)
		sum = ((sum & 1) << 7) + (sum >> 1) + *filename++;
	return sum;
}

void create_file_entry(char* filename, file_entry* fe, int is_dir)
{
	for (int i = 0; i < 8; i++) {
		if (i < strlen(filename))
			fe->msdos.filename[i] = filename[i];
		else
			fe->msdos.filename[i] = ' ';
	}
	if (filename[0] == 0xE5)
		fe->msdos.filename[0] = 0x05;

	char* extension = 0;
	if (strrchr(filename, '.') != NULL) {
		extension = strrchr(filename, '.');
		*extension = 0, extension++;
	}

	for (int i = 0; i < 3; i++) {
		if (extension && i < strlen(extension))
			fe->msdos.extension[i] = extension[i];
		else
			fe->msdos.extension[i] = ' ';
	}
	fe->msdos.attributes = 0x20 | (is_dir << 4);
	fe->msdos.reserved = 0;

	time_t rawtime;
	struct tm* timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime); // or gmtime()
	fe->msdos.creationDate = fe->msdos.modifiedDate =
		((timeinfo->tm_year - 80) << 9) | ((timeinfo->tm_mon << 5) + 1) | (timeinfo->tm_mday);
	fe->msdos.creationTime = fe->msdos.modifiedTime =
		((timeinfo->tm_hour << 11) | (timeinfo->tm_min << 5) | (timeinfo->tm_sec >> 1));

	fe->msdos.eaIndex = 0;
	fe->msdos.firstCluster = 0;
	fe->msdos.fileSize = 0;

	uint8_t checksum = checksum_filename(fe->msdos.filename);

	fe->lfnc = strlen(filename) / 13 + 1;
	fe->lfn_list = malloc(fe->lfnc * sizeof(FatFileLFN));;	
	uint8_t seq_num = 1;
	for (int i = 0; i < fe->lfnc; i++) {
		fe->lfn_list[fe->lfnc - i - 1].sequence_number = seq_num, seq_num++;
		fe->lfn_list[fe->lfnc - i - 1].attributes = 0x0F;
		fe->lfn_list[fe->lfnc - i - 1].firstCluster = 0x00;
		fe->lfn_list[fe->lfnc - i - 1].checksum = checksum;
		fe->lfn_list[fe->lfnc - i - 1].reserved = 0;
		for (int j = 0; j < 5; j++) {
			if (strlen(filename) < j + i * 13)
				fe->lfn_list[fe->lfnc - i - 1].name1[j] = 0xFFFF;
			else
				fe->lfn_list[fe->lfnc - i - 1].name1[j] = filename[j + i * 13];
		}

		for (int j = 0; j < 6; j++) {
			if (strlen(filename) < j + 5 + i * 13)
				fe->lfn_list[fe->lfnc - i - 1].name2[j] = 0xFFFF;
			else
				fe->lfn_list[fe->lfnc - i - 1].name2[j] = filename[j + 5 + i * 13];
		}

		for (int j = 0; j < 2; j++) {
			if (strlen(filename) < j + 11 + i * 13)
				fe->lfn_list[fe->lfnc - i - 1].name3[j] = 0xFFFF;
			else
				fe->lfn_list[fe->lfnc - i - 1].name3[j] = filename[j + 11 + i * 13];
		}
	}
	fe->lfn_list[0].sequence_number = (seq_num - 1) | 0x40;
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

	if (filename == NULL || strlen(filename) > 255)
		return;

	file_entry fe = {0};
	create_file_entry(filename, &fe, 0);
	write_file_entry(dir, &fe, 0, -1);

	free(fe.lfn_list);
	free(dir);
}

void mkdir(char* file)
{
	char* dir = strdup(file);
	char* dirname = strrchr(dir, '/');
	if (dirname != NULL) {
		*dirname = 0; 
		dirname++;
	} else {
		dirname = dir;
		dir = strdup(CWD);
	}

	if (dirname == NULL || strlen(dirname) > 255)
		return;

	file_entry fe = {0};
	create_file_entry(dirname, &fe, 1);
	write_file_entry(dir, &fe, 1, -1);

	free(fe.lfn_list);
	free(dir);
}

void mv(char* src, char* dst)
{
	file_entry* fe_src;
	int dir_i, dirs_read;
	uint32_t src_cluster = find_dir_cluster(src, &fe_src, &dir_i, &dirs_read, -1);
	if (!src_cluster)
		return;

	char* src_dir = strdup(src);
	char* src_filename = strrchr(src_dir, '/');
	if (src_filename != NULL) {
		*src_filename = 0; 
		src_filename++;
	} else {
		src_filename = src_dir;
		src_dir = strdup(CWD);
	}

	char* dst_dir = strdup(dst);
	char* dst_filename = strrchr(dst_dir, '/');
	if (dst_filename != NULL) {
		*dst_filename = 0;
		dst_filename++;
	} else {
		dst_filename = dst_dir;
		dst_dir = strdup(CWD);
	}

	if (dst_filename) {
		file_entry fe = {0};
		create_file_entry(dst_filename, &fe, 0);
		for (int i = 11; i; i--)
			fe_src[dir_i].msdos.filename[i] = fe.msdos.filename[i];
		free(fe_src[dir_i].lfn_list);
		fe_src[dir_i].lfnc = fe.lfnc;
		fe_src[dir_i].lfn_list = fe.lfn_list;

		free(dst_filename);
	}

	if (!strcmp(src_dir, dst_dir))
		write_file_entry(dst_dir, &fe_src[dir_i], 0, dir_i);
	else
		write_file_entry(dst_dir, &fe_src[dir_i], 0, -1);

	for (int i = 0; i < dirs_read; i++)
		free(fe_src[i].lfn_list);
	free(fe_src);
	free(dst_dir);
}
