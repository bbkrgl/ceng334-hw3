#include "filesystem.h"

int fs_fd = 0;
BPB_struct bpb;
FAT_entry** fat_table;
uint32_t CWD_cluster;
char* CWD;

char* months[] = {"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"};

inline static void handle_error(char* errmsg)
{
	fprintf(stderr, "%s:\n%s\n", errmsg, strerror(errno));
	exit(0);
}

void read_fat_tables()
{
	int rlseek = lseek(fs_fd, bpb.ReservedSectorCount * BPS, SEEK_SET);
	if (rlseek == -1)
		handle_error("Cannot read FAT tables");

	fat_table = malloc(sizeof(FAT_entry*) * bpb.NumFATs);
	for (int i = 0; i < bpb.NumFATs; i++) {
		fat_table[i] = malloc(bpb.extended.FATSize * BPS);
		int rcount = read(fs_fd, fat_table[i], bpb.extended.FATSize * BPS);
		if (rcount != bpb.extended.FATSize * BPS) {
			fprintf(stderr, "Cannot read FAT table %d.\nBytes read: %d, bytes expected: %d\n",
	   			i, rcount, bpb.extended.FATSize * BPS);
			exit(0);
		}
	}
}

int read_clusters(int fat_id, uint32_t cluster_num, void** data, int size)
{
	uint32_t curr_table_cluster = cluster_num;
	int i = 0;
	while ((i < size || size == -1)
		&& curr_table_cluster != 0xFFFFFFF
		&& curr_table_cluster != 0xFFFFFF8) {
		(*data) = realloc(*data, (i + 1) * bpb.SectorsPerCluster * BPS);

		int rlseek = lseek(fs_fd, (bpb.ReservedSectorCount +
			bpb.NumFATs * bpb.extended.FATSize +
			(curr_table_cluster - 2) * bpb.SectorsPerCluster) * BPS, SEEK_SET);

		if (rlseek == -1)
			handle_error("Cannot find cluster");

		int rcount = read(fs_fd, (*data) + i, bpb.SectorsPerCluster * BPS);
		if (rcount != bpb.SectorsPerCluster * BPS) {
			fprintf(stderr, "Cannot read cluster %d.\nBytes read: %d, bytes expected: %d\n",
				curr_table_cluster, rcount, bpb.extended.FATSize * BPS);
			exit(0);
		}

		curr_table_cluster = fat_table[fat_id][curr_table_cluster].address;
		i++;
	}

	return i;
}

int read_directory_table(int cluster_num, file_entry** directory) // TODO: Other FATs
{
	FatFileEntry* data = 0;
	int clusters_read = read_clusters(0, cluster_num, (void**) &data, -1);
	
	int files_read = 0;
	int parsed_lfn = 0;
	int entries_per_cluster = bpb.SectorsPerCluster * BPS / sizeof(FatFileEntry);
	for (int i = 0; i < clusters_read; i++) {
		for (int j = 0; j < entries_per_cluster; j++) {
			if (data[j + entries_per_cluster * i].lfn.sequence_number == 0xE5
				|| data[j + entries_per_cluster * i].lfn.sequence_number == 0) // What does 0xE5 mean???
				continue;

			if (data[j + entries_per_cluster * i].lfn.attributes != 0x0F) {
				if (!parsed_lfn) {
					*directory = realloc(*directory, 
			  			(files_read + 1) * sizeof(file_entry));

					(*directory)[files_read].lfn_list = 0;
				}

				(*directory)[files_read].msdos = data[j + entries_per_cluster * i].msdos;
				(*directory)[files_read].lfnc = parsed_lfn;
				files_read++;
				parsed_lfn = 0;
			} else {
				*directory = realloc(*directory, 
			 		(files_read + 1) * sizeof(file_entry));
				if (!parsed_lfn)
					(*directory)[files_read].lfn_list = 0;

				(*directory)[files_read].lfn_list = 
					realloc((*directory)[files_read].lfn_list, 
	     					(parsed_lfn + 1) * sizeof(FatFileLFN));

				(*directory)[files_read].lfn_list[parsed_lfn] = data[j + entries_per_cluster * i].lfn;
				parsed_lfn++;
			}
		}
	}

	free(data);
	return files_read;
}

int cmp_dirname(char* dirname, file_entry* fe, int is_dir)
{
	if (!(fe->msdos.attributes & 0x10) && is_dir)
		return 0;
	else if (fe->msdos.attributes & 0x10 && !is_dir)
		return 0;

	if (fe->lfnc == 0) {
		if (strlen(dirname) > 7)
			return 0;
		for (int i = 0; i < 7; i++) {
			if (fe->msdos.filename[i] == ' ' && strlen(dirname) < i)
				continue;
			if (strlen(dirname) < i || dirname[i] != fe->msdos.filename[i + 1])
				return 0;
		}
		return 1;
      	}

	for (int lfn_i = fe->lfnc - 1; lfn_i >= 0; lfn_i--) {
		for (int i = 0; i < 5; i++) {
			if (strlen(dirname) < lfn_i * 13 + i && fe->lfn_list[lfn_i].name1[i] == ' ')
				continue;
			if (strlen(dirname) < lfn_i * 13 + i ||
				dirname[lfn_i * 13 + i] != fe->lfn_list[lfn_i].name1[i])
				return 0;
		}
		
		if (fe->lfn_list[lfn_i].name2[0] == 0xFFFF)
			break;
		for (int i = 0; i < 6; i++) {
			if (strlen(dirname) < lfn_i * 13 + i + 5 &&
				(fe->lfn_list[lfn_i].name2[i] == ' ' || fe->lfn_list[lfn_i].name2[i] == 0xFFFF))
				continue;
			if (strlen(dirname) < lfn_i * 13 + i + 5 ||
				dirname[lfn_i * 13 + i + 5] != fe->lfn_list[lfn_i].name2[i])
				return 0;
		}

		if (fe->lfn_list[lfn_i].name3[0] == 0xFFFF)
			break;
		for (int i = 0; i < 2; i++) {
			if (strlen(dirname) < lfn_i * 13 + i + 11 &&
				(fe->lfn_list[lfn_i].name3[i] == ' ' || fe->lfn_list[lfn_i].name3[i] == 0xFFFF))
				continue;
			if (strlen(dirname) < lfn_i * 13 + i + 11 ||
				dirname[lfn_i * 13 + i + 11] != fe->lfn_list[lfn_i].name3[i])
				return 0;
		}
	}

	return 1;
}

int cmp_parent(char* dirname, file_entry* fe, uint32_t curr_cluster)
{
	if (fe->msdos.filename[0] != 0x2E || strcmp(dirname, ".."))
		return 0;
	
	int cl = (fe->msdos.eaIndex << 16) | fe->msdos.firstCluster;
	if (curr_cluster == cl)
		return 0;

	return 1;
}

uint32_t find_dir_cluster(char* dir, int is_dir)
{
	if (!strcmp(dir, "/"))
		return bpb.extended.RootCluster;
	if (!strcmp(dir, CWD))
		return CWD_cluster;
	else if (dir[strlen(dir) - 1] == '/' && !strncmp(dir, CWD, strlen(dir) - 1))
		return CWD_cluster;

	uint32_t dir_cluster;
	char* dir_cp = strdup(dir);
	char* dirname;
	if (dir_cp[0] == '/') {
		dir_cluster = bpb.extended.RootCluster;
		dirname = strsep(&dir_cp, "/");
	} else {
		dir_cluster = CWD_cluster;
	}

	while ((dirname = strsep(&dir_cp, "/")) != NULL) {
		if (!strlen(dirname))
			break;
		file_entry* fe = 0;
		int dirs_read = read_directory_table(dir_cluster, &fe); // TODO: Change size to -1 etc.
		int dir_found = 0;
		int dir_i = 0;
		int is_curr_dir = is_dir;
		if (!(strrchr(dir, '/') && !strcmp(dirname, strrchr(dir, '/') + 1)))
			is_curr_dir = is_dir;

		for (; dir_i < dirs_read; dir_i++) {
			if (cmp_dirname(dirname, &fe[dir_i], is_curr_dir)
				|| cmp_parent(dirname, &fe[dir_i], dir_cluster)) {
				dir_found = 1;
				break;
			}
		}

		if (!dir_found)
			return 0;

		dir_cluster = (fe[dir_i].msdos.eaIndex << 16) | fe[dir_i].msdos.firstCluster;
		if (dir_cluster == 0)
			dir_cluster = bpb.extended.RootCluster;

		free(fe->lfn_list);
		free(fe);
	}
	free(dir_cp);

	return dir_cluster;
}

void open_fs(char* fsname)
{
	fs_fd = open(fsname, O_RDWR);
	if (fs_fd == -1)
		handle_error("Cannot open filesystem");

	int rcount = read(fs_fd, &bpb, sizeof(bpb));
	if (rcount != sizeof(bpb)) {
		fprintf(stderr, "Cannot read BPB.\nBytes read: %d, bytes expected: %d\n",
	  		rcount, (int) sizeof(bpb));
		exit(0);
	}

	CWD = strdup("/");
	CWD_cluster = bpb.extended.RootCluster;

	read_fat_tables();
}
