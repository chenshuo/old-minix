/* file - report on file type.		Author: Andy Tanenbaum */
/* Magic number detection changed to look-up table 08-Jan-91 - ajm */

#include <blocksize.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define XBITS 00111		/* rwXrwXrwX (x bits in the mode) */
#define ENGLISH 25		/* cutoff for determining if text is Eng. */
unsigned char buf[BLOCK_SIZE];

struct info {
  int match;			/* No of bytes to match for success */
  int execflag;			/* 1 == ack executable, 2 == gnu executable */
  unsigned char magic[4];	/* First four bytes of the magic number */
  char *description;		/* What it means */
} table[] = {
  0x03, 0x00, 0x1f, 0x9d, 0x8d, 0x00,
	"13-bit compressed file",
  0x03, 0x00, 0x1f, 0x9d, 0x90, 0x00,
	"16-bit compressed file",
  0x02, 0x00, 0x65, 0xff, 0x00, 0x00,
	"MINIX-PC bcc archive",
  0x02, 0x00, 0x2c, 0xff, 0x00, 0x00, 
	"MINIX-68k ack archive",
  0x02, 0x00, 0x65, 0xff, 0x00, 0x00, 
	"MINIX-PC ack archive",
  0x04, 0x00, 0x47, 0x6e, 0x75, 0x20, 
	"MINIX-68k gnu archive",
  0x04, 0x00, 0x21, 0x3c, 0x61, 0x72, 
	"MINIX-PC gnu archive",
  0x02, 0x00, 0x01, 0x02, 0x00, 0x00, 
	"MINIX-68k ack object file",
  0x02, 0x00, 0xa3, 0x86, 0x00, 0x00, 
	"MINIX-PC bcc object file",
  0x04, 0x00, 0x00, 0x00, 0x01, 0x07, 
	"MINIX-68k gnu object file",
  0x04, 0x00, 0x07, 0x01, 0x00, 0x00, 
	"MINIX-PC gnu object file",
  0x04, 0x01, 0x01, 0x03, 0x10, 0x04, 
	"MINIX-PC 16-bit executable combined I & D space",
  0x04, 0x01, 0x01, 0x03, 0x20, 0x04, 
	"MINIX-PC 16-bit executable separate I & D space",
  0x04, 0x01, 0x01, 0x03, 0x20, 0x10, 
	"MINIX-PC 32-bit executable combined I & D space",
  0x04, 0x01, 0x01, 0x03, 0x10, 0x10, 
	"MINIX-PC 32-bit executable separate I & D space",
  0x04, 0x01, 0x04, 0x10, 0x03, 0x01, 
	"MINIX-68k old style executable",
  0x04, 0x01, 0x01, 0x03, 0x10, 0x0b, 
	"MINIX-68k new style executable",
  0x04, 0x02, 0x0b, 0x01, 0x00, 0x00, 
	"MINIX-PC 32-bit gnu executable combined I & D space",
  0x04, 0x02, 0x00, 0x00, 0x0b, 0x01, 
	"MINIX-68k gnu executable"
};

int tabsize = sizeof(table) / sizeof(struct info);

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void file, (char *name));
_PROTOTYPE(void do_strip, (int type));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
/* This program uses some heuristics to try to guess about a file type by
 * looking at its contents.
 */

  int i;

  if (argc < 2) usage();
  for (i = 1; i < argc; i++) file(argv[i]);
  return(0);
}

void file(name)
char *name;
{
  int i, fd, n, mode, nonascii, special, funnypct, etaoins;
  int j, matches;
  long engpct;
  int c;
  struct stat st_buf;

  printf("%s: ", name);

  /* Open the file, stat it, and read in 1 block. */
  fd = open(name, O_RDONLY);
  if (fd < 0) {
	printf("cannot open\n");
	return;
  }
  n = fstat(fd, &st_buf);
  if (n < 0) {
	printf("cannot stat\n");
	close(fd);
	return;
  }
  mode = st_buf.st_mode;

  /* Check for directories and special files. */
  if ((mode & S_IFMT) == S_IFDIR) {
	printf("directory\n");
	close(fd);
	return;
  }
  if ((mode & S_IFMT) == S_IFCHR) {
	printf("character special file\n");
	close(fd);
	return;
  }
  if ((mode & S_IFMT) == S_IFBLK) {
	printf("block special file\n");
	close(fd);
	return;
  }
  n = read(fd, (char *)buf, BLOCK_SIZE);
  if (n < 0) {
	printf("cannot read\n");
	close(fd);
	return;
  }
  if (n == 0) {       /* must check this, for loop will fail otherwise !! */
      printf("empty file\n");
      close(fd);
      return;
  }

  for (i = 0; i < tabsize; i++) {
	matches = 0;
	for (j = 0; j < table[i].match; j++)
		if (buf[j] == table[i].magic[j])
			matches++;
	if (matches == table[i].match) {
		printf("%s", table[i].description);
		do_strip(table[i].execflag);
		close(fd);
		return;
		}
  }


  /* Check to see if file is a shell script. */
  if (mode & XBITS) {
	/* Not a binary, but executable.  Probably a shell script. */
	printf("shell script\n");
	close(fd);
	return;
  }

  /* Check for ASCII data and certain punctuation. */
  nonascii = 0;
  special = 0;
  etaoins = 0;
  for (i = 0; i < n; i++) {
	c = buf[i];
	if (c & 0200) nonascii++;
	if (c == ';' || c == '{' || c == '}' || c == '#') special++;
	if (c == '*' || c == '<' || c == '>' || c == '/') special++;
	if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
	if (c == 'e' || c == 't' || c == 'a' || c == 'o') etaoins++;
	if (c == 'i' || c == 'n' || c == 's') etaoins++;
  }

  if (nonascii == 0) {
	/* File only contains ASCII characters.  Continue processing. */
	funnypct = 100 * special / n;
	engpct = 100L * (long) etaoins / n;
	if (funnypct > 1) {
		printf("C program\n");
	} else {
		if (engpct > (long) ENGLISH)
			printf("English text\n");
		else
			printf("ASCII text\n");
	}
	close(fd);
	return;
  }

  /* Give up.  Call it data. */
  printf("data\n");
  close(fd);
  return;
}

void do_strip(type)
int type;
{
  if (type == 1) {	/* Non-GNU executable */
	if (( buf[28] | buf[29] | buf[30] | buf[31]) != 0)
		printf(" not stripped\n");
	else
		printf(" stripped\n");
	return;
  }

  if (type == 2) {	/* GNU format executable */
     if ((buf[16] | buf[17] | buf[18] | buf[19]) != 0)
	 printf(" not stripped\n");
     else
	 printf(" stripped\n");
     return;
  }

  printf("\n");		/* Not an executable file */
 }

void usage()
{
  printf("Usage: file name ...\n");
  exit(1);
}
