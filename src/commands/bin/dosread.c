/* dos{read|write|dir} - handle DOS disks	Author: Michiel Huisjes */

/* dosdir - list MS-DOS directories.
 * doswrite - write stdin to DOS-file
 * dosread - read DOS-file to stdout
 *
 * Author: Michiel Huisjes.
 * 
 * Usage: dos... [-lra] drive [file/dir]
 *	  l: Give long listing.
 *	  r: List recursively.
 *	  a: Set ASCII bit.
 *
 *	Modified by Tim Kachel 4-88
 *		drive can be 0,1, a, b, c, d, e, f
 *		program will automatically configure to different hard disks
 *		  and the partitions for such (could change drive name for
 *		  a second hard disk if you have one)
 *			(has been tested on a 16 bit FAT AT drive)
 *			(High density AT diskettes and regular 360K)
 *		compile with cc -O -i
 *		hard disk is named /dev/hd0 to avoid accidents
 *		  To test FAT sizes on your hard disk first try dir -lr c
 *			(or what ever your dos partition is)  if this works
 *			properly then all the rest should be okay.
 *		If there are any problems there is debugging information
 *		  in fdinit() -- please let me know of any problems
 */

#include <sys/stat.h>

#define DRIVE0		"/dev/at0"
#define DRIVE1		"/dev/at1"
#define FDRIVE		"/dev/hd0"

#define DDDD	0xFD
#define DDHD	0xF9
#define DDFD	0xF8

#define	MAX_CLUSTER_SIZE	4096
#define MAX_FAT_SIZE		23552	/* 46 sectoren */
#define HMASK		0xFF00
#define LMASK		0x00FF

#define MAX_ROOT_ENTRIES	512	/* 32 sectoren */
#define FAT_START		512L	/* After bootsector */
#define clus_add(cl_no)		((long) (((long) cl_no - 2L) \
					* (long) cluster_size \
					+ (long) data_start \
				       ))
struct dir_entry {
	unsigned char d_name[8];
	unsigned char d_ext[3];
	unsigned char d_attribute;
	unsigned char d_reserved[10];
	unsigned short d_time;
	unsigned short d_date;
	unsigned short d_cluster;
	unsigned long d_size;
};

typedef struct dir_entry DIRECTORY;

#define NOT_USED	0x00
#define ERASED		0xE5
#define DIR		0x2E
#define DIR_SIZE	(sizeof (struct dir_entry))
#define SUB_DIR		0x10
#define NIL_DIR		((DIRECTORY *) 0)

#define LAST_CLUSTER	0x0FFF
#define MASK		0xFF8		/* FF8 - FFF are last cluster */
#define FREE		0x000
#define BAD		0xFF0		/* Includes reserved */

#define LAST_16		0xFFFF
#define MASK_16		0xFFF8
#define FREE_16		0x0000
#define BAD_16		0xFFF0		/* Includes reserved */

typedef char BOOL;

#define TRUE	1
#define FALSE	0
#define NIL_PTR	((char *) 0)

#define DOS_TIME	315532800L     /* 1970 - 1980 */

#define READ			0
#define WRITE			1
#define disk_read(s, a, b)	disk_io(READ, s, a, b)
#define disk_write(s, a, b)	disk_io(WRITE, s, a, b)

#define FIND	3
#define LABEL	4
#define ENTRY	5
#define find_entry(d, e, p)	directory(d, e, FIND, p)
#define list_dir(d, e, f)	(void) directory(d, e, f, NIL_PTR)
#define label()			directory(root, root_entries, LABEL, NIL_PTR)
#define new_entry(d, e)		directory(d, e, ENTRY, NIL_PTR)

#define is_dir(d)		((d)->d_attribute & SUB_DIR)

#define EOF			0400
#define EOF_MARK		'\032'
#define STD_OUT			1
#define flush()			print(STD_OUT, NIL_PTR, 0)

short disk;
union tbl
{
	unsigned char  twelve[4096];
	unsigned short sixteen[MAX_FAT_SIZE / 2 ];
} fat;

DIRECTORY root[MAX_ROOT_ENTRIES], save_entry, *directory(), *read_cluster();
char null[MAX_CLUSTER_SIZE], *device, path[128];
short total_clusters, cluster_size, fat_size, root_entries, sub_entries;

BOOL Rflag, Lflag, Aflag, dos_read, dos_write, dos_dir, Tfat = TRUE;

unsigned short free_cluster(), next_cluster();
char *make_name(), *num_out(), *slash(), *brk();
long mark, data_start, lseek(), time(), f_start;

leave(nr)
short nr;
{
	(void) umount(device);
	exit(nr);
}

usage(prog_name)
register char *prog_name;
{
	print_string(TRUE, "Usage: %s [%s\n", prog_name,
		     dos_dir ? "-lr] drive [dir]" : "-a] drive file");
	exit(1);
}

main(argc, argv)
int argc;
register char *argv[];
{
	register char *arg_ptr = slash(argv[0]);
	DIRECTORY *entry;
	short index = 1;
	char dev_nr;
	unsigned char fat_type, fat_check;
	BOOL fdisk = FALSE;
	int i;

	if (!strcmp(arg_ptr, "dosdir"))
		dos_dir = TRUE;
	else if (!strcmp(arg_ptr, "dosread"))
		dos_read = TRUE;
	else if (!strcmp(arg_ptr, "doswrite"))
		dos_write = TRUE;
	else {
		print_string(TRUE, "Program should be named dosread, doswrite or dosdir.\n");
		exit(1);
	}

	if (argc == 1)
		usage(argv[0]);

	if (argv[1][0] == '-') {
		for (arg_ptr = &argv[1][1]; *arg_ptr; arg_ptr++) {
			if (*arg_ptr == 'l' && dos_dir)
				Lflag = TRUE;
			else if (*arg_ptr == 'r' && dos_dir)
				Rflag = TRUE;
			else if (*arg_ptr == 'a' && !dos_dir)
				Aflag = TRUE;
			else
				usage(argv[0]);
		}
		index++;
	}

	if (index == argc)
		usage(argv[0]);

	switch (dev_nr = *argv[index++])
	{
		case '0':
		case 'a':	device = DRIVE0; break;
		case '1':
		case 'b':	device = DRIVE1; break;
		case 'c':
		case 'd':
		case 'e':
		case 'f':	fdisk = TRUE; device = FDRIVE; break;
		default :	usage(argv[0]);
	}

	if ((disk = open(device, 2)) < 0) {
		print_string(TRUE, "Cannot open %s\n", device);
		exit(1);
	}

	if (fdisk) {		/* fixed disk */
		fdinit(dev_nr);
		disk_read(f_start, &fat_type, sizeof(fat_type));
		if (fat_type != DDFD) {
			print_string(TRUE, "Fixed disk is not DOS\n");
			leave(1);
		}
	}
	else {		/* use standard start for floppies */
		f_start = FAT_START;
		disk_read(f_start, &fat_type, sizeof(fat_type));
		if (fat_type == DDDD) {		/* Double-sided double-density 9 s/t */
			total_clusters = 355;	/* 720 - 7 - 2 - 2 - 1 */
			cluster_size = 1024;	/* 2 sectors per cluster */
			fat_size = 1024;	/* 2 sectors */
			data_start = 6144L;	/* Starts on sector #12 */
			root_entries = 112;	
			sub_entries = 32;	/* 1024 / 32 */
		}
		else if (fat_type == DDHD) {	/* Double-sided high-density 15 s/t */
			total_clusters = 2372;	/* 2400 - 14 - 7 - 7 - 1 */
			cluster_size = 512;	/* 1 sector per cluster */
			fat_size = 3584;	/* 7 sectors */
			data_start = 14848L;	/* Starts on sector #29 */
			root_entries = 224;	
			sub_entries = 16;	/* 512 / 32 */
		}
		else {
        		print_string(TRUE, "Diskette is not DOS 2.0 360K or 1.2M\n");
			leave(1);
		}
	}

	disk_read(f_start + (long) fat_size, &fat_check, sizeof(fat_check));
	if (fat_check != fat_type) {
		print_string(TRUE, "Disk type in FAT copy differs from disk type in FAT original.\n");
		leave(1);
	}

	if (Tfat)	/* twelve bit FAT entries */
		disk_read(f_start, fat.twelve, fat_size);
	else		/* sixteen bit */
		disk_read(f_start, fat.sixteen, fat_size);
/*******
	for (i=0; i<= 30; i++){
		printf("%x\t%c", fat.sixteen[i], (i % 10) ? ' ':'\n');
	}
	leave(1);
/*******/
	disk_read(f_start + 2L * (long) fat_size, root, DIR_SIZE * root_entries);
/*******
	for (i=0; i<2; i++){
		printf("%s d_name\n", root[i].d_name);
		printf("%s d_ext\n",  root[i].d_ext);
		printf("%d d_attr\n",  root[i].d_attribute);
		printf("%s d_reserved\n",  root[i].d_reserved);
		printf("%d d_time\n",  root[i].d_time);
		printf("%d d_date\n",  root[i].d_date);
		printf("%d d_cluster\n",  root[i].d_cluster);
		printf("%D d_size\n",  root[i].d_size);
	}


/*********/
	if (dos_dir) {
		entry = label();
		print_string(FALSE, "Volume in drive %c ", dev_nr);
		if (entry == NIL_DIR)
			print(STD_OUT, "has no label.\n\n", 0);
		else
			print_string(FALSE, "is %S\n\n", entry->d_name);
	}

	if (argv[index] == NIL_PTR) {
		if (!dos_dir)
			usage(argv[0]);
		print(STD_OUT, "Root directory:\n", 0);
		list_dir(root, root_entries, FALSE);
		free_blocks();
		flush();
		leave(0);
	}

	for (arg_ptr = argv[index]; *arg_ptr; arg_ptr++)
		if (*arg_ptr == '\\')
			*arg_ptr = '/';
		else if (*arg_ptr >= 'a' && *arg_ptr <= 'z')
			*arg_ptr += ('A' - 'a');
	if (*--arg_ptr == '/')
		*arg_ptr = '\0';       /* skip trailing '/' */

	add_path(argv[index], FALSE);
	add_path("/", FALSE);

	if (dos_dir)
		print_string(FALSE, "Directory %s:\n", path);

	entry = find_entry(root, root_entries, argv[index]);

	if (dos_dir) {
		list_dir(entry, sub_entries, FALSE);
		free_blocks();
	}
	else if (dos_read)
		extract(entry);
	else {
		if (entry != NIL_DIR) {
			flush();
			if (is_dir(entry))
				print_string(TRUE, "%s is a directory.\n", path);
			else
				print_string(TRUE, "%s already exists.\n", argv[index]);
			leave(1);
		}

		add_path(NIL_PTR, TRUE);

		if (*path)
			make_file(find_entry(root, root_entries, path),
				  sub_entries, slash(argv[index]));
		else
			make_file(root, root_entries, argv[index]);
	}

	(void) close(disk);
	flush();
	leave(0);
}

fdinit(part_nr)		/* Fixed Disk Initializations */
char part_nr;
{

#define	SECSIZE        512        /* sector size        */
#define TABLEOFFSET    0x1be      /* offset in boot sector*/

	/*
	 * Description of entry in partition table
	 */
	struct part_entry {
	    char    bootind;    /* boot indicator 0/0x80    */
	    char    start_head;    /* head value for first sector    */
	    char    start_sec;    /* sector value for first sector*/
	    char    start_cyl;    /* track value for first sector    */
	    char    sysind;        /* system indicator 00=?? 01=DOS*/
	    char    last_head;    /* head value for last sector    */
	    char    last_sec;    /* sector value for last sector    */
	    char    last_cyl;    /* track value for last sector    */
	    long    lowsec;        /* logical first sector        */
	    long    size;        /* size of partion in sectors    */
	} *pe;

	char    secbuf[SECSIZE];

	/*
	 *	Description of the boot block
	 */
	struct {
		unsigned char jump[3];
		unsigned char oem[8];
		unsigned char bytes_sector[2];
		unsigned char cluster_size;
		unsigned char res_sectors[2];
		unsigned char num_fats;
		unsigned char root_entries[2];
		unsigned char logical_sectors[2];
		unsigned char media_type;
		unsigned char fat_sectors[2];
		unsigned char track_sectors[2];
		unsigned char num_heads[2];
		unsigned char hidden_sectors[2];
	} boot;

	short block_size, reserved;	long boot_loc;

	disk_read(0L, secbuf, SECSIZE);	/* get boot sector */
		/* offset into boot sector for the partition table */
	pe = (struct part_entry *)&secbuf[TABLEOFFSET];
		/* get the proper partition */
	switch(part_nr) {
		case 'f': pe++;
		case 'e': pe++;
		case 'd': pe++;
		case 'c': boot_loc = pe->lowsec * 512L; break;
        	default:  printf("Error: unknown partition\n"); leave();
	}
		/* now read the boot block for the partition needed */
	disk_read(boot_loc, &boot, sizeof(boot));

	/* this section can be used to print drive information */
/**************
	printf("OEM = %s\n", boot.oem);
	printf("Bytes/sector = %d\n",
		(boot.bytes_sector[1] << 8 & HMASK) + (boot.bytes_sector[0] & LMASK)); 
	printf("Sectors/cluster = %d\n", boot.cluster_size);
	printf("Number of Reserved Clusters = %d\n",
		(boot.res_sectors[1] << 8 & HMASK) + (boot.res_sectors[0] & LMASK));
	printf("Number of FAT's = %d\n", boot.num_fats);
	printf("Number of root-directory entries = %d\n",
		(boot.root_entries[1] << 8 & HMASK) + (boot.root_entries[0] & LMASK));
	printf("Total sectors in logical volume = %D\n",
		(long) (boot.logical_sectors[1] << 8 & HMASK) + (boot.logical_sectors[0] & LMASK));
	printf("Media Descriptor = %x\n", boot.media_type);
	printf("Number of sectors/FAT = %d\n",
		(boot.fat_sectors[1] << 8 & HMASK) + (boot.fat_sectors[0] & LMASK));
	printf("Sectors/track = %d\n",
		(boot.track_sectors[1] << 8 & HMASK) + (boot.track_sectors[0] & LMASK));
	printf("Number of Heads = %d\n",
		(boot.num_heads[1] << 8 & HMASK) + (boot.num_heads[0] & LMASK));
	printf("Number of hidden sectors = %d\n",
		(boot.hidden_sectors[1]  << 8 & HMASK) + (boot.hidden_sectors[0] & LMASK));
	leave(1);
/**************/
	if (boot.media_type != DDFD) {
		printf("DISK is not DOS Format.\n");
		leave(1);
	}
	if (boot.num_fats != 2) {
		printf("Disk does not have two FAT Tables!\n");
		leave(1);
	}
	block_size = (boot.bytes_sector[1] << 8 & HMASK) +
			 (boot.bytes_sector[0] & LMASK);
	if ((cluster_size = block_size * boot.cluster_size)
					> MAX_CLUSTER_SIZE) {
		printf("Cluster size is larger than MAX_CLUSTER_SIZE.\n");
		leave(1);
	}
	reserved =  ((boot.res_sectors[1] << 8 & HMASK) +
			(boot.res_sectors[0] & LMASK));
	f_start = boot_loc + (long) block_size * (long) reserved;
	root_entries = (boot.root_entries[1] << 8 & HMASK) +
			(boot.root_entries[0] & LMASK);
	fat_size = (boot.fat_sectors[1] << 8 & HMASK) +
			 (boot.fat_sectors[0] & LMASK);
		/* (sectors - rootdir - fats - reserved) / blocks/cluster */
	total_clusters = (int) ((long) ((boot.logical_sectors[1] << 8 & HMASK) +
				   (boot.logical_sectors[0] & LMASK)) - 
		  (root_entries * 32 / block_size) -
		  (fat_size * 2) - reserved) / boot.cluster_size;
	if (total_clusters > 4096)
		Tfat = FALSE;		/* sixteen bit fat entries */
	if ( (fat_size *= block_size) > MAX_FAT_SIZE) {
		printf("Disk FAT is larger than MAX_FAT_SIZE.\n");
		leave(1);
	}
	sub_entries = cluster_size / 32;
	data_start = f_start + (long) (fat_size * 2L) +
			(long) (root_entries * 32L);
/**********
	printf("f_start = %D\n", f_start);
	printf("total_clusters = %d\n", total_clusters);
	printf("cluster_size = %d\n", cluster_size);
	printf("fat_size = %d\n", fat_size);
	printf("data_start = %D\n", data_start);
	printf("root_entries = %d\n", root_entries);
	printf("sub_entries = %d\n", sub_entries);
	printf("Tfat = %d\n", Tfat);
	leave(1);
/*********/
}

DIRECTORY *directory(dir, entries, function, pathname)
DIRECTORY *dir;
short entries;
BOOL function;
register char *pathname;
{
	register DIRECTORY *dir_ptr = dir;
	DIRECTORY *mem = NIL_DIR;
	unsigned short cl_no = dir->d_cluster;
	unsigned short type, last;
	char file_name[14];
	char *name;
	int i = 0;

	if (function == FIND) {
        while (*pathname != '/' && *pathname && i < 12)
			file_name[i++] = *pathname++;
		while (*pathname != '/' && *pathname)
			pathname++;
		file_name[i] = '\0';
	}

	do {
		if (entries != root_entries) {
			mem = dir_ptr = read_cluster(cl_no);
			last = cl_no;
			cl_no = next_cluster(cl_no);
		}

		for (i = 0; i < entries; i++, dir_ptr++) {
			type = dir_ptr->d_name[0] & 0x0FF;
			if (function == ENTRY) {
				if (type == NOT_USED || type == ERASED) {
					mark = lseek(disk, 0L, 1) -
						(long) cluster_size +
						(long) i * (long) DIR_SIZE;
					if (!mem)
						mark += (long) cluster_size - (long) (root_entries * sizeof (DIRECTORY));
					return dir_ptr;
				}
				continue;
			}
			if (type == NOT_USED)
				break;
			if (dir_ptr->d_attribute & 0x08) {
				if (function == LABEL)
					return dir_ptr;
				continue;
			}
			if (type == DIR || type == ERASED || function == LABEL)
				continue;
			type = is_dir(dir_ptr);
			name = make_name(dir_ptr, (function == FIND) ?
					 FALSE : type);
			if (function == FIND) {
				if (strcmp(file_name, name) != 0)
					continue;
				if (!type) {
					if (dos_dir || *pathname) {
						flush();
						print_string(TRUE, "Not a directory: %s\n", file_name);
						leave(1);
					}
				}
				else if (*pathname == '\0' && dos_read) {
					flush();
					print_string(TRUE, "%s is a directory.\n", path);
					leave(1);
				}
				if (*pathname) {
					dir_ptr = find_entry(dir_ptr,
						   sub_entries, pathname + 1);
				}
				if (mem) {
					if (dir_ptr) {
						bcopy(dir_ptr, &save_entry, DIR_SIZE);
						dir_ptr = &save_entry;
					}
					(void) brk(mem);
				}
				return dir_ptr;
			}
			else {
				if (function == FALSE)
					show(dir_ptr, name);
				else if (type) {	/* Recursive */
					print_string(FALSE, "Directory %s%s:\n", path, name);
					add_path(name, FALSE);
					list_dir(dir_ptr, sub_entries, FALSE);
					add_path(NIL_PTR, FALSE);
				}
			}
		}
		if (mem)
			(void) brk(mem);
	} while ((Tfat && cl_no != LAST_CLUSTER && mem) ||
		 (!Tfat && cl_no != LAST_16 && mem));

	switch (function) {
		case FIND:
			if (dos_write && *pathname == '\0')
				return NIL_DIR;
			flush();
			print_string(TRUE, "Cannot find `%s'.\n", file_name);
			leave(1);
		case LABEL:
			return NIL_DIR;
		case ENTRY:
			if (!mem) {
				flush();
				print_string(TRUE, "No entries left in root directory.\n");
				leave(1);
			}

			cl_no = free_cluster(TRUE);
			link_fat(last, cl_no);
			if (Tfat)
				link_fat(cl_no, LAST_CLUSTER);
			else
				link_fat(cl_no, LAST_16);
			disk_write(clus_add(cl_no), null, cluster_size);

			return new_entry(dir, entries);
		case FALSE:
			if (Rflag) {
				print(STD_OUT, "\n", 0);
				list_dir(dir, entries, TRUE);
			}
	}
}

extract(entry)
register DIRECTORY *entry;
{
	register unsigned short cl_no = entry->d_cluster;
	char buffer[MAX_CLUSTER_SIZE];
	short rest;

	if (entry->d_size == 0)	       /* Empty file */
		return;

	do {
		disk_read(clus_add(cl_no), buffer, cluster_size);
		rest = (entry->d_size > (long) cluster_size) ? cluster_size : (short) entry->d_size;
		print(STD_OUT, buffer, rest);
		entry->d_size -= (long) rest;
		cl_no = next_cluster(cl_no);
		if ((Tfat && cl_no == BAD) || (!Tfat && cl_no == BAD_16)){
			flush();
			print_string(TRUE, "Reserved cluster value encountered.\n");
			leave(1);
		}
	} while ((Tfat && entry->d_size && cl_no != LAST_CLUSTER) ||
		 (!Tfat && entry->d_size && cl_no != LAST_16));

	if ((Tfat && cl_no != LAST_CLUSTER) || (!Tfat && cl_no != LAST_16))
		print_string(TRUE, "Too many clusters allocated for file.\n");
	else if (entry->d_size != 0)
		print_string(TRUE, "Premature EOF: %L bytes left.\n",
			     entry->d_size);
}

print(fd, buffer, bytes)
short fd;
register char *buffer;
register short bytes;
{
	static short index;
	static BOOL lf_pending = FALSE;
	static char output[MAX_CLUSTER_SIZE + 1];

	if (buffer == NIL_PTR) {
		if (dos_read && Aflag && lf_pending) {
			output[index++] = '\r';
			lf_pending = FALSE;
		}
		if (write(fd, output, index) != index)
			bad();
		index = 0;
		return;
	}

	if (bytes == 0)
		bytes = strlen(buffer);

	while (bytes--) {
		if (index >= MAX_CLUSTER_SIZE) {
			if (write(fd, output, index) != index)
				bad ();
			index = 0;
		}
		if (dos_read && Aflag) {
			if (*buffer == '\r') {
				if (lf_pending)
					output[index++] = *buffer++;
				else {
					lf_pending = TRUE;
					buffer++;
				}
			}
			else if (*buffer == '\n') {
				output[index++] = *buffer++;
				lf_pending = FALSE;
			}
			else if (lf_pending) {
				output[index++] = '\r';
				output[index++] = *buffer++;
			}
			else if ((output[index++] = *buffer++) == EOF_MARK) {
				if (lf_pending) {
					output[index - 1] = '\r';
					index++;
					lf_pending = FALSE;
				}
				index--;
				return;
			}
		}
		else
			output[index++] = *buffer++;
	}
}

make_file(dir_ptr, entries, name)
DIRECTORY *dir_ptr;
int entries;
char *name;
{
	register DIRECTORY *entry = new_entry(dir_ptr, entries);
	register char *ptr;
	char buffer[MAX_CLUSTER_SIZE];
	unsigned short cl_no, next;
	short i, r;
	long size = 0L;

    bcopy("           ",&entry->d_name[0],11);    /* clear entry */
    for (i = 0, ptr = name; i < 8 && *ptr != '.' && *ptr; i++)
        entry->d_name[i] = *ptr++;
    while (*ptr != '.' && *ptr)
        ptr++;
    if (*ptr == '.')
        ptr++;
    for (i=0;i < 3 && *ptr; i++)
        entry->d_ext[i] = *ptr++;

	for (i = 0; i < 10; i++)
		entry->d_reserved[i] = '\0';
	entry->d_attribute = '\0';

	entry->d_cluster = 0;

	while ((r = fill(buffer)) > 0) {
		if ((next = free_cluster(FALSE)) > total_clusters) {
			print_string(TRUE, "Disk full. File truncated.\n");
			break;
		}

		disk_write(clus_add(next), buffer, r);

		if (entry->d_cluster == 0)
			cl_no = entry->d_cluster = next;
		else {
			link_fat(cl_no, next);
			cl_no = next;
		}

		size += r;
	}

	if (entry->d_cluster != 0) {
		if (Tfat)
			link_fat(cl_no, LAST_CLUSTER);
		else
			link_fat(cl_no, LAST_16);
	}

	entry->d_size = Aflag ? (size - 1) : size;	/* Strip added ^Z */
	fill_date(entry);
	disk_write(mark, entry, DIR_SIZE);
	if (Tfat) {
		disk_write(f_start, fat.twelve, fat_size);
		disk_write(f_start + (long) fat_size, fat.twelve, fat_size);
	} else {
		disk_write(f_start, fat.sixteen, fat_size);
		disk_write(f_start + (long) fat_size, fat.sixteen, fat_size);
	}
}


#define SEC_MIN	60L
#define SEC_HOUR	(60L * SEC_MIN)
#define SEC_DAY	(24L * SEC_HOUR)
#define SEC_YEAR	(365L * SEC_DAY)
#define SEC_LYEAR	(366L * SEC_DAY)

short mon_len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

fill_date(entry)
DIRECTORY *entry;
{
	register long cur_time = time((long *) 0) - DOS_TIME;
	unsigned short year = 0, month = 1, day, hour, minutes, seconds;
	int i;
	long tmp;

	if (cur_time < 0)	       /* Date not set on booting ... */
		cur_time = 0;
	for (;;) {
		tmp = (year % 4 == 0) ? SEC_LYEAR : SEC_YEAR;
		if (cur_time < tmp)
			break;
		cur_time -= tmp;
		year++;
	}

	day = (unsigned short) (cur_time / SEC_DAY);
	cur_time -= (long) day *SEC_DAY;

	hour = (unsigned short) (cur_time / SEC_HOUR);
	cur_time -= (long) hour *SEC_HOUR;

	minutes = (unsigned short) (cur_time / SEC_MIN);
	cur_time -= (long) minutes *SEC_MIN;

	seconds = (unsigned short) cur_time;

	mon_len[1] = (year % 4 == 0) ? 29 : 28;
	i = 0;
	while (day >= mon_len[i]) {
		month++;
        day -= mon_len[i++];
	}
	day++;

	entry->d_date = (year << 9) | (month << 5) | day;
	entry->d_time = (hour << 11) | (minutes << 5) | seconds;
}

char *make_name(dir_ptr, dir_fl)
register DIRECTORY *dir_ptr;
short dir_fl;
{
	static char name_buf[14];
	register char *ptr = name_buf;
	short i;

	for (i = 0; i < 8; i++)
		*ptr++ = dir_ptr->d_name[i];

	while (*--ptr == ' ');

	ptr++;
	if (dir_ptr->d_ext[0] != ' ') {
		*ptr++ = '.';
		for (i = 0; i < 3; i++)
			*ptr++ = dir_ptr->d_ext[i];
		while (*--ptr == ' ');
		ptr++;
	}
	if (dir_fl)
		*ptr++ = '/';
	*ptr = '\0';

	return name_buf;
}

fill(buffer)
register char *buffer;
{
	static BOOL eof_mark = FALSE;
	char *last = &buffer[cluster_size];
	char *begin = buffer;
	register short c;

	if (eof_mark)
		return 0;

	while (buffer < last) {
		if ((c = get_char()) == EOF) {
			eof_mark = TRUE;
			if (Aflag)
				*buffer++ = EOF_MARK;
			break;
		}
		*buffer++ = c;
	}

    return (int) (buffer - begin);
}

get_char()
{
	static short read_chars, index;
	static char input[MAX_CLUSTER_SIZE];
	static BOOL new_line = FALSE;

	if (new_line == TRUE) {
		new_line = FALSE;
		return '\n';
	}

	if (index == read_chars) {
		if ((read_chars = read(0, input, cluster_size)) == 0)
			return EOF;
		index = 0;
	}

	if (Aflag && input[index] == '\n') {
		new_line = TRUE;
		index++;
		return '\r';
	}

	return input[index++];
}

#define HOUR	0xF800		       /* Upper 5 bits */
#define MIN	0x07E0		       /* Middle 6 bits */
#define YEAR	0xFE00		       /* Upper 7 bits */
#define MONTH	0x01E0		       /* Mid 4 bits */
#define DAY	0x01F		       /* Lowest 5 bits */

char *month[] = {
		 "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

modes(mode)
register unsigned char mode;
{
	print_string(FALSE, "\t%c%c%c%c%c", (mode & SUB_DIR) ? 'd' : '-',
		     (mode & 02) ? 'h' : '-', (mode & 04) ? 's' : '-',
		     (mode & 01) ? '-' : 'w', (mode & 0x20) ? 'a' : '-');
}

show(dir_ptr, name)
DIRECTORY *dir_ptr;
char *name;
{
	register unsigned short e_date = dir_ptr->d_date;
	register unsigned short e_time = dir_ptr->d_time;
	unsigned short next;
	char bname[20];
	short i = 0;

	while (*name && *name != '/')
		bname[i++] = *name++;
	bname[i] = '\0';
	if (!Lflag) {
		print_string(FALSE, "%s\n", bname);
		return;
	}
	modes(dir_ptr->d_attribute);
	print_string(FALSE, "\t%s%s", bname, strlen(bname) < 8 ? "\t\t" : "\t");
	i = 1;
	if (is_dir(dir_ptr)) {
		next = dir_ptr->d_cluster;
		while (((next = next_cluster(next)) != LAST_CLUSTER && Tfat) ||
			(!Tfat && next != LAST_16))
			i++;
		print_string(FALSE, "%L", (long) i * (long) cluster_size);
	}
	else
		print_string(FALSE, "%L", dir_ptr->d_size);
	print_string(FALSE, "\t%N:%N %P %s %d\n", ((e_time & HOUR) >> 11),
		     ((e_time & MIN) >> 5), (e_date & DAY),
	   month[((e_date & MONTH) >> 5) - 1], ((e_date & YEAR) >> 9) + 1980);
}

free_blocks()
{
	register unsigned short cl_no;
	register short free = 0;
	short bad = 0;
	if (Tfat)
		for (cl_no = 2; cl_no <= total_clusters; cl_no++) {
			switch (next_cluster(cl_no)) {
				case FREE:
					free++;
					break;
				case BAD:
					bad++;
			}
		}
	else
		for (cl_no = 2; cl_no <= total_clusters; cl_no++) {
			switch (next_cluster(cl_no)) {
				case FREE_16:
					free++;
					break;
				case BAD_16:
					bad++;
			}
		}

	print_string(FALSE, "Free space: %L bytes.\n", (long) free * (long) cluster_size);
	if (bad)
		print_string(FALSE, "Bad sectors: %L bytes.\n", (long) bad * (long) cluster_size);
}

char *num_out(number)
register long number;
{
	static char num_buf[13];
	char temp[13];
	register short i = 0;
	short j;

	if (number == 0)
		temp[i++] = '0';

	while (number) {
		temp[i++] = (char) (number % 10L + '0');
		number /= 10L;
	}

	for (j = 0; j < 11; j++)
		num_buf[j] = temp[i - j - 1];

	num_buf[i] = '\0';
	return num_buf;
}

/* VARARGS */
print_string(err_fl, fmt, args)
BOOL err_fl;
char *fmt;
int args;
{
	char buf[200];
	register char *buf_ptr = buf;
	char *scan_ptr;
	register int *arg_ptr = &args;
	short i;

	while (*fmt) {
		if (*fmt == '%') {
			fmt++;
			if (*fmt == 'c') {
				*buf_ptr++ = (char) *arg_ptr++;
				fmt++;
				continue;
			}
			if (*fmt == 'S') {
				scan_ptr = (char *) *arg_ptr;
				for (i = 0; i < 11; i++)
					*buf_ptr++ = *scan_ptr++;
				fmt++;
				continue;
			}
			if (*fmt == 's')
				scan_ptr = (char *) *arg_ptr;
			else if (*fmt == 'L') {
				scan_ptr = num_out(*((long *) arg_ptr));
				arg_ptr++;
			}
			else {
				scan_ptr = num_out((long) *arg_ptr);
				if (*fmt == 'P' && *arg_ptr < 10)
					*buf_ptr++ = ' ';
				else if (*fmt == 'N' && *arg_ptr < 10)
					*buf_ptr++ = '0';
			}
			while (*buf_ptr++ = *scan_ptr++);
			buf_ptr--;
			arg_ptr++;
			fmt++;
		}
		else
			*buf_ptr++ = *fmt++;
	}

	*buf_ptr = '\0';

	if (err_fl) {
		flush();
        write(2, buf, (int) (buf_ptr - buf));
	}
	else
		print(STD_OUT, buf, 0);
flush();
}

DIRECTORY *read_cluster(cluster)
register unsigned short cluster;
{
	register DIRECTORY *sub_dir;
	extern char *sbrk();

	if ((sub_dir = (DIRECTORY *) sbrk(cluster_size)) < 0) {
		print_string(TRUE, "Cannot set break!\n");
		leave(1);
	}
	disk_read(clus_add(cluster), sub_dir, cluster_size);

	return sub_dir;
}

unsigned short free_cluster(leave_fl)
BOOL leave_fl;
{
	static unsigned short cl_index = 2;

	if (Tfat)
		while (cl_index <= total_clusters && next_cluster(cl_index) != FREE)
			cl_index++;
	else		/* Sixteen bit */
		while (cl_index <= total_clusters && next_cluster(cl_index) != FREE_16)
			cl_index++;

	if (leave_fl && cl_index > total_clusters) {
		flush();
		print_string(TRUE, "Disk full. File not added.\n");
		leave(1);
	}

	return cl_index++;
}

/* ****************FIX FOR SIXTEEN BIT ***************** */
link_fat(cl_1, cl_2)
unsigned short cl_1;
register unsigned short cl_2;
{
	if (Tfat) {
		register unsigned char *fat_index = &fat.twelve[(cl_1 >> 1) * 3 + 1];
		if (cl_1 & 0x01) {
			*(fat_index + 1) = cl_2 >> 4;
			*fat_index = (*fat_index & 0x0F) | ((cl_2 & 0x0F) << 4);
		}
		else {
			*(fat_index - 1) = cl_2 & 0x0FF;
			*fat_index = (*fat_index & 0xF0) | (cl_2 >> 8);
		}
	}
	else {
		fat.sixteen[cl_1] = cl_2;
	}
}


unsigned short next_cluster(cl_no)
register unsigned short cl_no;
{
	if (Tfat) {
		register unsigned char *fat_index = &fat.twelve[(cl_no >> 1) * 3 + 1];

		if (cl_no & 0x01)
			cl_no = (*(fat_index + 1) << 4) | (*fat_index >> 4);
		else
			cl_no = ((*fat_index & 0x0F) << 8) | *(fat_index - 1);

		if ((cl_no & MASK) == MASK)
			cl_no = LAST_CLUSTER;
		else if ((cl_no & BAD) == BAD)
			cl_no = BAD;
	}
	else {
		/*cl_no = fat.sixteen[cl_no << 1];*/
		cl_no = fat.sixteen[cl_no];
		if ((cl_no & MASK_16) == MASK_16)
			cl_no = LAST_16;
		else if ((cl_no & BAD_16) == BAD_16)
			cl_no = BAD_16;
	}

	return cl_no;
}

char *slash(str)
register char *str;
{
	register char *result = str;

	while (*str)
		if (*str++ == '/')
			result = str;

	return result;
}

add_path(file, slash_fl)
register char *file;
BOOL slash_fl;
{
	register char *ptr = path;

	while (*ptr)
		ptr++;

	if (file == NIL_PTR) {
		ptr--;
		do {
			ptr--;
		} while (*ptr != '/' && ptr != path);
		if (ptr != path && !slash_fl)
			ptr++;
		*ptr = '\0';
	}
	else
		while (*ptr++ = *file++);
}

bcopy(src, dest, bytes)
register char *src, *dest;
short bytes;
{
	while (bytes--)
		*dest++ = *src++;
}

disk_io(op, seek, address, bytes)
register BOOL op;
unsigned long seek;
DIRECTORY *address;
register unsigned bytes;
{
	unsigned int r;

	if (lseek(disk, seek, 0) < 0L) {
		flush();
		print_string(TRUE, "Bad lseek\n");
		leave(1);
	}

	if (op == READ)
		r = read(disk, address, bytes);
	else
		r = write(disk, address, bytes);

	if (r != bytes)
		bad();
}

bad()
{
	flush();
	perror("I/O error");
	leave(1);
}
