#include "filesystem.h"

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

int read_cluster(int fat_id, uint32_t cluster_num, void** data, int size)
{	
	uint32_t curr_table_cluster = cluster_num;
	int i = 0;
	while (curr_table_cluster != 0xFFFFFFF && i < size) {
		int rlseek = lseek(fs_fd, (bpb.ReservedSectorCount +
			    bpb.NumFATs * bpb.extended.FATSize +
			    (curr_table_cluster - 2) * bpb.SectorsPerCluster) * BPS, SEEK_SET); // TODO: Fix
		if (rlseek == -1)
			handle_error("Cannot read file");

		int rcount = read(fs_fd, data[i], bpb.SectorsPerCluster * BPS);
		if (rcount != bpb.SectorsPerCluster * BPS) {
			fprintf(stderr, "Cannot read cluster %d.\nBytes read: %d, bytes expected: %d\n",
	   			i, rcount, bpb.extended.FATSize * BPS);
			exit(0);
		}
		
		curr_table_cluster = fat_table[fat_id][curr_table_cluster].address;
		i++;
	}

	return i;
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

	read_fat_tables();

	FatFileEntry* cluster = malloc(bpb.SectorsPerCluster * BPS);
	void** data = (void**) &cluster;
	read_cluster(0, bpb.extended.RootCluster, data, 1);

	// TODO: Fix
	for (int i = 0; i < 5; i++)
		printf("%c", cluster[2].lfn.name1[i]);
	printf("\n");
}
