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

void seek_data_cluster(uint32_t cluster)
{
	if (cluster == 0)
		cluster = bpb.extended.RootCluster;

	int rlseek = lseek(fs_fd, (bpb.ReservedSectorCount +
		bpb.NumFATs * bpb.extended.FATSize +
		(cluster - 2) * bpb.SectorsPerCluster) * BPS, SEEK_SET);

	if (rlseek == -1)
		handle_error("Cannot find cluster");
}

int read_clusters(int fat_id, uint32_t cluster_num, void** data, int size)
{
	uint32_t curr_table_cluster = cluster_num;
	int i = 0;
	while ((i < size || size == -1) && curr_table_cluster < 0xFFFFFF8) {
		(*data) = realloc(*data, (i + 1) * bpb.SectorsPerCluster * BPS);

		seek_data_cluster(curr_table_cluster);
		int rcount = read(fs_fd,
			(void*) ((uint64_t)(*data) + i * bpb.SectorsPerCluster * BPS),
			bpb.SectorsPerCluster * BPS);
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

int read_directory_table(int cluster_num, file_entry** directory)
{
	FatFileEntry* data = 0;
	int clusters_read = read_clusters(0, cluster_num, (void**) &data, -1);

	int files_read = 0;
	int parsed_lfn = 0;
	int entries_per_cluster = bpb.SectorsPerCluster * BPS / sizeof(FatFileEntry);
	for (int i = 0; i < clusters_read; i++) {
		for (int j = 0; j < entries_per_cluster; j++) {
			if (data[j + entries_per_cluster * i].lfn.sequence_number == 0xE5
				|| data[j + entries_per_cluster * i].lfn.sequence_number == 0)
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
	if (!(fe->msdos.attributes & 0x10) && is_dir == 1)
		return 0;
	else if (fe->msdos.attributes & 0x10 && is_dir == 0)
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

	int str_i = 0;
	for (int lfn_i = fe->lfnc - 1; lfn_i >= 0; lfn_i--) {
		for (int i = 0; i < 5; i++) {
			uint16_t c = fe->lfn_list[lfn_i].name1[i];
			if (c == 0xFFFF || c == ' ' || c == 0)
				continue;
			if (dirname[str_i] != c)
				return 0;
			if (str_i++ >= strlen(dirname))
				return 0;
		}

		for (int i = 0; i < 6; i++) {
			uint16_t c = fe->lfn_list[lfn_i].name2[i];
			if (c == 0xFFFF || c == ' ' || c == 0)
				continue;
			if (dirname[str_i] != c)
				return 0;
			if (str_i++ >= strlen(dirname))
				return 0;
		}
		
		for (int i = 0; i < 2; i++) {
			uint16_t c = fe->lfn_list[lfn_i].name3[i];
			if (c == 0xFFFF || c == ' ' || c == 0)
				continue;
			if (dirname[str_i] != c)
				return 0;
			if (str_i++ >= strlen(dirname))
				return 0;
		}
	}

	return 1;
}

int cmp_parent(char* dirname, file_entry* fe)
{
	if (fe->msdos.filename[0] != 0x2E ||
		fe->msdos.filename[1] != 0x2E ||
		strcmp(dirname, ".."))
		return 0;

	return 1;
}

uint32_t find_dir_cluster(char* dir, file_entry **fe_return_parent, int* dir_i_return, int* dirs_read_return, int is_dir)
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
		if (!strlen(dirname)) {
			break;
		} else if (!strcmp(dirname, ".")) {
			dir_cluster = CWD_cluster;
			continue;
		}

		file_entry* fe = 0;
		int dirs_read = read_directory_table(dir_cluster, &fe);
		int dir_found = 0;
		int dir_i = 0;
		int is_curr_dir = 1;
		if (!(strrchr(dir, '/') && !strcmp(dirname, strrchr(dir, '/') + 1)))
			is_curr_dir = is_dir;

		for (; dir_i < dirs_read; dir_i++) {
			if (cmp_dirname(dirname, &fe[dir_i], is_curr_dir)
				|| cmp_parent(dirname, &fe[dir_i])) {
				dir_found = 1;
				break;
			}
		}

		if (!dir_found) {
			free(fe->lfn_list);
			free(fe);
			//free(dir_cp);
			if (fe_return_parent)
				*fe_return_parent = 0;
			return 0;
		}

		dir_cluster = (fe[dir_i].msdos.eaIndex << 16) | fe[dir_i].msdos.firstCluster;
		if (dir_cluster == 0)
			dir_cluster = bpb.extended.RootCluster;

		if (!fe_return_parent) {
			free(fe->lfn_list);
			free(fe);
		} else {
			*fe_return_parent = fe;
			*dir_i_return = dir_i;
			*dirs_read_return = dirs_read;
		}
	}

	free(dir_cp);
	return dir_cluster;
}

void flush_fat_table(int alloc_cl)
{
	int curr_pos = lseek(fs_fd, 0, SEEK_CUR);
	int rlseek = lseek(fs_fd, bpb.ReservedSectorCount * BPS, SEEK_SET);
	if (rlseek == -1)
		handle_error("Cannot find FAT tables");

	int size = bpb.extended.FATSize * BPS;
	for (int i = 0; i < bpb.NumFATs; i++) {
		int wcount = write(fs_fd, fat_table[i], size);
		if (wcount != size) {
			fprintf(stderr, "Cannot write FT entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, size);
		}
	}

	if (alloc_cl > 0) {
		rlseek = lseek(fs_fd, bpb.extended.FSInfo * BPS + 0x1E8, SEEK_SET);
		int free_cl;
		int rrcount = read(fs_fd, &free_cl, 4);
		if (rrcount != 4)
			handle_error("Cannot read free cluster count");
		free_cl -= alloc_cl;
		rlseek = lseek(fs_fd, bpb.extended.FSInfo * BPS + 0x1E8, SEEK_SET);
		int wcount = write(fs_fd, &free_cl, 4);
		if (rrcount != 4)
			handle_error("Cannot write free cluster count");
	}

	rlseek = lseek(fs_fd, curr_pos, SEEK_SET);
	if (rlseek == -1)
		handle_error("Cannot return to original position");
}

uint32_t allocate_clusters(int num_entries, int flush)
{
	uint32_t first_cluster = 0;
	int cluster = 0, old_cluster = 0, i = 0;
	int num_clusters = bpb.extended.FATSize * BPS / sizeof(FAT_entry);
	for (; cluster < num_clusters && i < num_entries; cluster++) {
		if (fat_table[0][cluster].address == 0) {
			if (!i) {
				first_cluster = cluster;
				old_cluster = cluster;
				for (int j = 0; j < bpb.NumFATs; j++)
					fat_table[j][cluster].address = 0xFFFFFF8;
				i++;
				continue;
			}

			for (int j = 0; j < bpb.NumFATs; j++) {
				fat_table[j][old_cluster].address = cluster;
				fat_table[j][cluster].address = 0xFFFFFF8;
			}
			old_cluster = cluster;
			i++;
		}
	}

	if (flush)
		flush_fat_table(i);

	return first_cluster;
}

int sizeof_dir_entry(file_entry* fe, int dir_num)
{
	int size = 0;
	for (int i = 0; i < dir_num; i++)
		size += fe[i].lfnc * sizeof(FatFileLFN) + sizeof(FatFile83);
	return size;
}

uint32_t get_last_cluster(uint32_t cluster)
{
	uint32_t lc = cluster;
	while (fat_table[0][lc].address < 0xFFFFFF8) {
		lc = fat_table[0][lc].address;
	}
	return lc;
}

void write_directory(uint32_t dir_cluster, file_entry* fe_dir, int dir_size)
{
	int entries_written = 0;
	int entries_per_cluster = bpb.BytesPerSector * BPS / sizeof(FatFileEntry);

	seek_data_cluster(dir_cluster);
	for (int i = 0; i < dir_size; i++) {
		for (int j = 0; j < fe_dir[i].lfnc; j++) {
			int wcount = write(fs_fd, &fe_dir[i].lfn_list[j], sizeof(FatFileLFN));
			if (wcount != sizeof(FatFileLFN)) {
				fprintf(stderr, "Cannot write LFN entry.\nBytes read: %d, bytes expected: %d\n",
					wcount, (int) sizeof(FatFileLFN));
			}
			entries_written++;
			if (entries_written >= entries_per_cluster) {
				entries_written = 0;
				dir_cluster = fat_table[0][dir_cluster].address;
				seek_data_cluster(dir_cluster);
			}
		}

		int wcount = write(fs_fd, &fe_dir[i].msdos, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}
		entries_written++;
		if (entries_written >= entries_per_cluster) {
			entries_written = 0;
			dir_cluster = fat_table[0][dir_cluster].address;
			seek_data_cluster(dir_cluster);
		}
	}
}

void write_file_entry(char* dir, file_entry* fe, int create_dir)
{
	uint32_t dir_cluster = find_dir_cluster(dir, 0, 0, 0, 1);
	if (!dir_cluster)
		return;

	uint32_t first_dir_cluster = dir_cluster;
	file_entry* fe_dir = 0;
	int dirs_read = read_directory_table(dir_cluster, &fe_dir);

	int initial_size = sizeof_dir_entry(fe_dir, dirs_read);
	int new_size = initial_size + sizeof_dir_entry(fe, 1);
	int clusters_needed = new_size / (bpb.SectorsPerCluster * BPS) -
		initial_size / (bpb.SectorsPerCluster * BPS);
	if (clusters_needed) {
		uint32_t alloc_cluster = allocate_clusters(clusters_needed, 0);
		uint32_t lc = get_last_cluster(dir_cluster);
		for (int i = 0; i < bpb.NumFATs; i++)
			fat_table[i][lc].address = alloc_cluster;
		flush_fat_table(alloc_cluster);
	}

	int entries_written = 0;
	int entries_per_cluster = bpb.SectorsPerCluster * BPS / sizeof(FatFileEntry);
	seek_data_cluster(dir_cluster);
	for (int i = 0; i < dirs_read; i++) {
		for (int j = 0; j < fe_dir[i].lfnc; j++) {
			int wcount = write(fs_fd, &fe_dir[i].lfn_list[j], sizeof(FatFileLFN));
			if (wcount != sizeof(FatFileLFN)) {
				fprintf(stderr, "Cannot write LFN entry.\nBytes read: %d, bytes expected: %d\n",
					wcount, (int) sizeof(FatFileLFN));
			}
			entries_written++;
			if (entries_written >= entries_per_cluster) {
				entries_written = 0;
				dir_cluster = fat_table[0][dir_cluster].address;
				seek_data_cluster(dir_cluster);
			}
		}

		int wcount = write(fs_fd, &fe_dir[i].msdos, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}
		entries_written++;
		if (entries_written >= entries_per_cluster) {
			entries_written = 0;
			dir_cluster = fat_table[0][dir_cluster].address;
			seek_data_cluster(dir_cluster);
		}
	}
	for (int j = 0; j < fe->lfnc; j++) {
		int wcount = write(fs_fd, &fe->lfn_list[j], sizeof(FatFileLFN));
		if (wcount != sizeof(FatFileLFN)) {
			fprintf(stderr, "Cannot write LFN entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFileLFN));
		}
		entries_written++;
		if (entries_written >= entries_per_cluster) {
			entries_written = 0;
			dir_cluster = fat_table[0][dir_cluster].address;
			seek_data_cluster(dir_cluster);
		}
	}

	if (create_dir == 1) {
		uint32_t newdir_cluster = allocate_clusters(1, 1);
		fe->msdos.eaIndex = newdir_cluster >> 16;
		fe->msdos.firstCluster = newdir_cluster & 0xFFFF;
		int wcount = write(fs_fd, &fe->msdos, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}

		FatFile83 fe_dot = fe->msdos;
		for (int i = 11; i; i--)
			fe_dot.filename[i] = ' ';
		fe_dot.filename[0] = 0x2E;
		fe_dot.attributes = 0x30;
		seek_data_cluster(newdir_cluster);
		wcount = write(fs_fd, &fe_dot, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}

		fe_dot.filename[1] = 0x2E;
		if (first_dir_cluster == bpb.extended.RootCluster)
			first_dir_cluster = 0;
		fe_dot.eaIndex = first_dir_cluster >> 16;
		fe_dot.firstCluster = first_dir_cluster & 0xFFFF;
		wcount = write(fs_fd, &fe_dot, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}
	} else if (create_dir == -1) {
		int wcount = write(fs_fd, &fe->msdos, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}

		dir_cluster = (fe->msdos.eaIndex << 16) | fe->msdos.firstCluster;
		seek_data_cluster(dir_cluster);
		
		int rlseek = lseek(fs_fd, sizeof(FatFile83) + offsetof(FatFile83, eaIndex), SEEK_CUR);
		if (rlseek == -1)
			handle_error("Cannot seek to msdos entry.");
		if (first_dir_cluster == bpb.extended.RootCluster)
			first_dir_cluster = 0;
		uint16_t eaIndex = first_dir_cluster >> 16;
		wcount = write(fs_fd, &eaIndex, sizeof(uint16_t));
		if (wcount != sizeof(uint16_t)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(uint16_t));
		}
		
		rlseek = lseek(fs_fd, 2 * sizeof(uint16_t), SEEK_CUR);
		if (rlseek == -1)
			handle_error("Cannot seek to msdos entry.");
		uint16_t firstCluster = first_dir_cluster & 0xFFFF;
		wcount = write(fs_fd, &firstCluster, sizeof(uint16_t));
		if (wcount != sizeof(uint16_t)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(uint16_t));
		}
	} else {
		int wcount = write(fs_fd, &fe->msdos, sizeof(FatFile83));
		if (wcount != sizeof(FatFile83)) {
			fprintf(stderr, "Cannot write msdos entry.\nBytes read: %d, bytes expected: %d\n",
				wcount, (int) sizeof(FatFile83));
		}
	}
}

void open_fs(char* fsname)
{
	fs_fd = open(fsname, O_RDWR);
	if (fs_fd == -1)
		handle_error("Cannot open filesystem");

	int rcount = read(fs_fd, &bpb, sizeof(BPB_struct));
	if (rcount != sizeof(bpb)) {
		fprintf(stderr, "Cannot read BPB.\nBytes read: %d, bytes expected: %d\n",
	  		rcount, (int) sizeof(BPB_struct));
		exit(0);
	}

	CWD = strdup("/");
	CWD_cluster = bpb.extended.RootCluster;

	read_fat_tables();
}
