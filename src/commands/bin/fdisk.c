/* fdisk - partition a hard disk	Author: Jakob Schripsema */

/*
 * First run the DOS utilities FDISK and FORMAT. FDISK
 * puts the boot code in sector 0.
 * Then run fdisk
 *
 *	fdisk /dev/hdx	(MINIX)
 *	fdisk x:		(DOS)
 *
 * Compiling
 *
 *	cc -o fdisk -DUNIX fdisk.c	(MINIX)
 *	cl -DDOS fdisk.c			(DOS with MS C compiler)
 */
 
#include <stdio.h>
#define UNIX		/* for MINIX */

#ifdef DOS
#include <dos.h>
#endif

/*
 * Constants
 */

#define	NHEAD		4		/* # heads		*/
#define	NSEC		17		/* sectors / track	*/
#define SECSIZE		512		/* sector size		*/
#define	OK		0
#define	ERR		1
#define	TABLEOFFSET	0x1be		/* offset in boot sector*/
/*
 * Description of entry in partition table
 */

struct part_entry {
	char	bootind;	/* boot indicator 0/0x80	*/
	char	start_head;	/* head value for first sector	*/
	char	start_sec;	/* sector value for first sector*/
	char	start_cyl;	/* track value for first sector	*/
	char	sysind;		/* system indicator 00=?? 01=DOS*/
	char	last_head;	/* head value for last sector	*/
	char	last_sec;	/* sector value for last sector	*/
	char	last_cyl;	/* track value for last sector	*/
	long	lowsec;		/* logical first sector		*/
	long	size;		/* size of partion in sectors	*/
};
	
/*
 * Globals
 */

char	secbuf[SECSIZE];
char	*devname;
char	*dosstr  = "  DOS  ";
char	*ndosstr = "Non-DOS";

#ifdef DOS
union	REGS	regs;
struct	SREGS	sregs;
int	drivenum;
#endif

#ifdef UNIX
int	devfd;
#endif

main(argc,argv)
int	argc;
char	*argv[];
{
	char	ch;

	/* init */

	if (argc != 2) {
		printf("Usage: fdisk /dev/hdx\n");
		exit(1);
	}

	devname = argv[1];
	getboot(secbuf);	/* get boot sector	*/

	do {
		dpl_partitions();
		print_menu();
		ch = get_a_char();
		putchar('\n');
		switch (ch) {
			case 'c' :
				change_table();
				break;
			case 'w' :
				if (chk_table() == OK) {
					putboot(secbuf);
					exit(0);
				}
				break;
			case 'l' :
				load_from_file();
				break;
			case 's' :
				save_to_file();
				break;
			case 'p' :
				dump_table();
				break;
			case 'q' :
				exit(0);
			default :
				printf(" %c ????\n",ch);
		}
	}
	while (1);
}

/*
 * Read boot sector
 */

#ifdef DOS

getboot(buffer)
char	*buffer;
{
	segread(&sregs);	/* get ds */

	if (devname[1] != ':') {
		printf("Invalid drive %s\n",devname);
		exit(1);
	}

	if (*devname >= 'a')
		*devname += 'A' - 'a';
	drivenum = (*devname - 'C') & 0xff;
	if (drivenum < 0 || drivenum > 7) {
		printf("Funny drive number %d\n",drivenum);
		exit(1);
	}
	regs.x.ax = 0x201;	/* read 1 sectors	*/
	regs.h.ch = 0;		/* track		*/
	regs.h.cl = 1;		/* first sector = 1	*/
	regs.h.dh = 0;		/* head = 0		*/
	regs.h.dl = 0x80+drivenum;/* drive = 0		*/
	sregs.es = sregs.ds;	/* buffer address	*/
	regs.x.bx = (int)secbuf;

	int86x(0x13,&regs,&regs,&sregs);
	if (regs.x.cflag)
	{
		printf("Cannot read boot sector\n");
		exit(1);
	}
}
#endif

#ifdef UNIX

getboot(buffer)
char	*buffer;
{
	devfd = open(devname,2);
	if (devfd < 0) {
		printf("Cannot open device %s\n",devname);
		exit(1);
	}
	if (read(devfd,buffer,SECSIZE) != SECSIZE) {
		printf("Cannot read boot sector\n");
		exit(2);
	}
}
#endif

/*
 * Write boot sector
 */

#ifdef DOS

putboot(buffer)
char	*buffer;
{
	regs.x.ax = 0x301;	/* read 1 sectors	*/
	regs.h.ch = 0;		/* track		*/
	regs.h.cl = 1;		/* first sector = 1	*/
	regs.h.dh = 0;		/* head = 0		*/
	regs.h.dl = 0x80+drivenum;/* drive = 0		*/
	sregs.es = sregs.ds;	/* buffer address	*/
	regs.x.bx = (int)secbuf;

	int86x(0x13,&regs,&regs,&sregs);
	if (regs.x.cflag)
	{
		printf("Cannot write boot sector\n");
		exit(1);
	}
}
#endif

#ifdef UNIX

putboot(buffer)
char	*buffer;
{
	int r;

	if (lseek(devfd,0L,0) < 0) {
		printf("Seek error during write\n");
		exit(1);
	}
	if (write(devfd,buffer,SECSIZE) != SECSIZE) {
		printf("Write error\n");
		exit(1);
	}
	sync();
}
#endif

/*
 * Load buffer from file
 */

load_from_file()
{
	char	file[80];
	int	fd;

	printf("Enter file name: ");
	gets(file);
#ifdef UNIX
	fd = open(file,0);
#endif
#ifdef DOS
	fd = open(file,0x8000);
#endif
	if (fd < 0) {
		fprintf(stderr,"Cannot load from %s\n",file);
		exit(1);
	}
	if (read(fd,secbuf,SECSIZE) != SECSIZE) {
		fprintf(stderr,"Read error\n");
		exit(1);
	}
	close(fd);
}

/*
 * Save to file
 */

save_to_file()
{
	char	file[80];
	int	fd;

	printf("Enter file name: ");
	gets(file);
#ifdef UNIX
	fd = creat(file,0644);
#endif
#ifdef DOS
	fd = creat(file,0644);
	if (fd < 0) {
		fprintf(stderr,"Cannot creat %s\n",file);
		exit(1);
	}
	close(fd);
	fd = open(file,0x8001);
#endif
	if (fd < 0) {
		fprintf(stderr,"Cannot save to %s\n",file);
		exit(1);
	}
	if (write(fd,secbuf,SECSIZE) != SECSIZE) {
		fprintf(stderr,"Write error\n");
		exit(1);
	}
	close(fd);
}

/*
 * Dump partition table
 */

dump_table()
{
	struct	part_entry	*pe;
	int	i;

	pe = (struct part_entry *)&secbuf[TABLEOFFSET];
	printf("\n       --first---     ---last---\n");
	printf("Prt ac hd sec cyl sys hd sec cyl    low      size\n");
	for (i = 1 ; i < 5 ; i++) {
		printf(" %x  %2x  %x  %2x  %2x  %2x  %x  %2x  %2x %6D %9D\n",
			i,
			pe->bootind & 0xff,
			pe->start_head & 0xff,
			pe->start_sec & 0xff,
			pe->start_cyl & 0xff,
			pe->sysind & 0xff,
			pe->last_head & 0xff,
			pe->last_sec & 0xff,
			pe->last_cyl & 0xff,
			pe->lowsec,
			pe->size);
		pe++;
	}
}
/*
 * Display partition table
 */

dpl_partitions()
{
	struct	part_entry	*pe;
	int	i;

	printf("\nPartition      Type     Begin End  Active\n");
	pe = (struct part_entry *)&secbuf[TABLEOFFSET];
	for (i = 1 ; i <= 4 ; i++) {
		dpl_entry(i,pe);
		pe++;
	}
}

/*
 * Display an entry
 */

dpl_entry(number,entry)
int	number;
struct	part_entry	*entry;
{
	int	low_cyl,high_cyl,temp;
	char	*typestring;
	char	active;

	if (entry->sysind == 0x01)
		typestring = dosstr;
	else
		typestring = ndosstr;
	printf("%5d         %s  ",number,typestring);
	temp = entry->start_sec & 0xc0;
	low_cyl = (entry->start_cyl & 0xff) + (temp << 2);
	temp = entry->last_sec & 0xc0;
	high_cyl = (entry->last_cyl & 0xff) + (temp << 2);
	printf("%4d  %4d",low_cyl,high_cyl);
	if ((entry->bootind & 0xff) == 0x80)
		active = 'A';
	else
		active = 'N';
	printf("     %c\n",active);
}

/*
 * Check partition table
 */

chk_table()
{
	struct part_entry *pe;
	int i;
	int active;
	long limit;

	pe = (struct part_entry *)&secbuf[TABLEOFFSET];
	limit = 0L;
	active = 0;
	for (i = 1 ; i < 5 ; i++) {
		if (pe->size == 0L)
			return(OK);
		if (pe->lowsec <= limit) {
			printf("Overlap between part. %d and %d\n",i,i-1);
			return(ERR);
		}
		limit = pe->lowsec + pe->size - 1L;
		if (pe->bootind == 0x80)
			active++;
		pe++;
	}
	if (active > 1) {
		printf("%d active partitions\n",active);
		return(ERR);
	}
	return(OK);
}
/*
 * Check entry
 * head/sector/track info must match logical sector number
 * Used to check 'foreign' partition table during debugging
 */

chk_entry(entry)
struct	part_entry	*entry;
{
	char	head;
	char	sector;
	char	track;

	sec_to_hst(entry->lowsec,&head,&sector,&track);
	if (	(head != entry->start_head) ||
		(sector != entry->start_sec) ||
		(track != entry->start_cyl))
		return(ERR);
	sec_to_hst(entry->lowsec + entry->size - 1L,&head,&sector,&track);
	if (	(head != entry->last_head) ||
		(sector != entry->last_sec) ||
		(track != entry->last_cyl))
		return(ERR);
	return(OK);
}

/*
 * Convert a logical sector number to
 * head / sector / track
 */

sec_to_hst(logsec,hd,sec,cyl)
long	logsec;
char	*hd,*sec,*cyl;
{
	int	bigcyl;

	bigcyl = logsec / (NHEAD * NSEC);
	*sec = (logsec % NSEC) + 1 + ((bigcyl >> 2) & 0xc0);
	*cyl = bigcyl & 0xff;
	*hd = (logsec % (NHEAD * NSEC)) / NSEC;
}

/*
 * change partition table
 */

change_table()
{
	struct	part_entry	*pe;
	int	i,temp,low_cyl,high_cyl;
	char	ch;

	pe = (struct part_entry *)&secbuf[TABLEOFFSET];
	for (i = 1 ; i <= 4 ; i++) {
		temp = pe->start_sec & 0xc0;
		low_cyl = (pe->start_cyl & 0xff) + (temp << 2);
		temp = pe->last_sec & 0xc0;
		high_cyl = (pe->last_cyl & 0xff) + (temp << 2);
		printf("Partition %d from %d to %d. Change? (y/n) ",
			i,low_cyl,high_cyl);
		ch = get_a_char();
		if (ch == 'y' || ch == 'Y')
			get_partition(pe);
		pe++;
	}
	putchar('\n');
}

/*
 * Get partition info : first & last cylinder
 */

get_partition(entry)
struct part_entry *entry;
{
	char	buf[80];
	int	first,last;
	long	low,high;
	char	ch;

	printf("	Enter first cylinder: ");
	gets(buf);
	sscanf(buf,"%d",&first);
	printf("	Enter last cylinder: ");
	gets(buf);
	sscanf(buf,"%d",&last);;
	if (first == 0 && last == 0) {
		entry->bootind = 0;
		entry->start_head = 0;
		entry->start_sec = 0;
		entry->start_cyl = 0;
		entry->sysind = 0;
		entry->last_head = 0;
		entry->last_sec  = 0;
		entry->last_cyl = 0;
		entry->lowsec = 0L;
		entry->size = 0L ;
		return;
	}
	low = first & 0xffff;
	low = low * NSEC * NHEAD;
	if (low == 0)
		low = 1; /* sec0 is master boot record */
	high = last & 0xffff;
	high = (high + 1)*NSEC*NHEAD - 1;
	entry->lowsec = low;
	entry->size = high - low + 1;
	sec_to_hst(low,
		&entry->start_head,
		&entry->start_sec,
		&entry->start_cyl);
	sec_to_hst(high,
		&entry->last_head,
		&entry->last_sec,
		&entry->last_cyl);
	printf("	DOS partition? (y/n) ");
	ch = get_a_char();
	if (ch == 'y' || ch == 'Y')
		entry->sysind = 1;
	else
		entry->sysind = 0;
	printf("	Active partition? (y/n) ");
	ch = get_a_char();
	if (ch == 'y' || ch == 'Y')
		entry->bootind = 0x80;
	else
		entry->bootind = 0;
}

/*
 * Read 1 character and discard rest of line
 */

get_a_char()
{
	char	ch;

	ch = getchar();
	if (ch != '\n')
		while (getchar() != '\n');
	return(ch);
}

print_menu()
{
  printf("\nType a command letter, then a carriage return:\n");
  printf("   c - change a partition\n");
  printf("   w - write changed partition table back to disk and exit\n");
  printf("   q - quit without making any changes\n");
  printf("   s - save boot block (including partition table) on a file\n");
  printf("   l - load boot block (including partition table) from a file\n");
  printf("   p - print partition table\n\n");
}
