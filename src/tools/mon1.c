/* mon1.c 1.4.9 - Load and start Minix.		Author: Kees J. Bot */

/* This program has more features than are officially supported. If
 * EXTENDED_LIST is defined, the help menu lists additional options.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <a.out.h>

#include "minix/config.h"
#include "minix/const.h"
#include "minix/partition.h"
#include "minix/boot.h"
#include "minix/type.h"
#include "minix/syslib.h"
#include "kernel/const.h"
#include "kernel/type.h"

#undef EXTERN
#define EXTERN			/* Empty */

#define _MONHEAD
#include <tools.h>

#ifndef _ANSI
#undef NULL
#define NULL 0
#endif

#define B_NODEV		-1
#define B_NOSIG		-2

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))
#define b_getenv(name)	(*searchenv(name))
#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))
#define	printf	printk

#define DEV_HD1a (DEV_HD0 + 128)/* First subpartition /dev/hd1a. */

typedef struct token {
  struct token *next;		/* Next in a command chain. */
  char *token;
} token;

#define CACHE_SIZE	32	/* More than enough. */

int cache_live = 0;

struct cache_entry {
  u32_t block;
  u16_t seg;
} cache[CACHE_SIZE];

enum whatfun { NOFUN, SELECT, DEFFUN, USERFUN };

_PROTOTYPE(char *bios_err, (int err ));
_PROTOTYPE(char *unix_err, (int err ));
_PROTOTYPE(void rwerr, (char *rw, off_t sec, int err ));
_PROTOTYPE(void writerr, (off_t sec, int err ));
_PROTOTYPE(void init_cache, (void));
_PROTOTYPE(void invalidate_cache, (void));
_PROTOTYPE(void readblock, (off_t blk, char *buf ));
_PROTOTYPE(char *readline, (void));
_PROTOTYPE(int sugar, (char *tok ));
_PROTOTYPE(char *onetoken, (char **aline, int arg ));
_PROTOTYPE(token **tokenize, (token **acmds, char *line, int *fundef ));
_PROTOTYPE(char *poptoken, (void));
_PROTOTYPE(void voidtoken, (void));
_PROTOTYPE(void void_cmds, (void));
_PROTOTYPE(void interrupt, (void));
_PROTOTYPE(void migrate, (void));
_PROTOTYPE(void partsort, (struct part_entry *table ));
_PROTOTYPE(int get_master, (char *master, struct part_entry **tbl, u32_t pos));
_PROTOTYPE(void initialize, (void));
_PROTOTYPE(void sfree, (char *s ));
_PROTOTYPE(char *copystr, (char *s ));
_PROTOTYPE(int is_default, (environment *e ));
_PROTOTYPE(environment **searchenv, (char *name ));
_PROTOTYPE(char *b_body, (char *name ));
_PROTOTYPE(int b_setenv, (int flags, char *name, char *arg, char *value ));
_PROTOTYPE(int b_setvar, (int flags, char *name, char *value ));
_PROTOTYPE(void b_unset, (char *name ));
_PROTOTYPE(void get_parameters, (void));
_PROTOTYPE(void addparm, (char **ap, char *n ));
_PROTOTYPE(void save_parameters, (void));
_PROTOTYPE(void show_env, (void));
_PROTOTYPE(int exec_bootstrap, (void));
_PROTOTYPE(void boot_device, (char *devname ));
_PROTOTYPE(void ls, (char *dir ));
_PROTOTYPE(void unschedule, (void));
_PROTOTYPE(void schedule, (long msec, char *cmd ));
_PROTOTYPE(int expired, (void));
_PROTOTYPE(int delay, (char *msec ));
_PROTOTYPE(enum whatfun menufun, (environment *e ));
_PROTOTYPE(void menu, (void));
_PROTOTYPE(void execute, (void));
_PROTOTYPE(void monitor, (void));
_PROTOTYPE(void boot, (void));
_PROTOTYPE(void disp_cmds, (void));

char *bios_err(err)
int err;
/* Translate BIOS error code to a readable string.  (This is a rare trait
 * known as error checking and reporting.  Take a good look at it, you won't
 * see it often.)
 */
{
  static struct errlist {
	unsigned char err;
	char *what;
  } errlist[] = {
	{ 0x00, "No error" },
	{ 0x01, "Invalid command" },
	{ 0x02, "Address mark not found" },
	{ 0x03, "Disk write-protected" },
	{ 0x04, "Sector not found" },
	{ 0x05, "Reset failed" },
	{ 0x06, "Floppy disk removed" },
	{ 0x07, "Bad parameter table" },
	{ 0x08, "DMA overrun" },
	{ 0x09, "DMA crossed 64 KB boundary" },
	{ 0x0A, "Bad sector flag" },
	{ 0x0B, "Bad track flag" },
	{ 0x0C, "Media type not found" },
	{ 0x0D, "Invalid number of sectors on format" },
	{ 0x0E, "Control data address mark detected" },
	{ 0x0F, "DMA arbitration level out of range" },
	{ 0x10, "Uncorrectable CRC or ECC data error" },
	{ 0x11, "ECC corrected data error" },
	{ 0x20, "Controller failed" },
	{ 0x40, "Seek failed" },
	{ 0x80, "Disk timed-out" },
	{ 0xAA, "Drive not ready" },
	{ 0xBB, "Undefined error" },
	{ 0xCC, "Write fault" },
	{ 0xE0, "Status register error" },
	{ 0xFF, "Sense operation failed" }
  };
  struct errlist *errp;

  for (errp = errlist; errp < arraylimit(errlist); errp++)
	if (errp->err == err) return(errp->what);
  return("Unknown error");
}

char *unix_err(err)
int err;
{
/* Translate the few errors rawfs can give. */
  switch (err) {
      case ENOENT:
	return("No such file or directory");
      case ENOTDIR:
	return("Not a directory");
      default:	return("Unknown error");
  }
}

void rwerr(rw, sec, err)
char *rw;
off_t sec;
int err;
{
  printf("\n%s error 0x%02x (%s) at sector %ld absolute\n",
         rw, err, bios_err(err), sec);
}

void readerr(sec, err)
off_t sec;
int err;
{
  rwerr("Read", sec, err);
}

void writerr(sec, err)
off_t sec;
int err;
{
  rwerr("Write", sec, err);
}

/* Readblock support for rawfs.c */
void init_cache()
{
/* Initialize the block cache. */
  struct cache_entry *pc;
  u16_t seg = FREESEG;

  for (pc = cache; pc < arraylimit(cache); pc++) {
	pc->block = -1;
	pc->seg = seg;
	seg += BLOCK_SIZE / HCLICK_SIZE;
  }

  cache_live = 1;		/* Turn it on. */
}

void invalidate_cache()
/* The cache can't be used when Minix is loaded. */
{
  cache_live = 0;
}

void readblock(blk, buf)
off_t blk;
char *buf;
{
/* Read blocks for the rawfs package with caching.  Wins 2 seconds. */
  int r = 0;
  u32_t sec = lowsec + blk * RATIO;

  if (!cache_live) {
	/* Cache invalidated, load block directly in place. */
	r = readsectors((u16_t) buf, dseg, sec, 1 * RATIO);
  } else {
	/* Search through the cache from 0 up.  Move the one found to
	 * the front of the cache, then optionally read a block. */
	struct cache_entry *pc, lifo, tmp;

	for (pc = cache; pc < arraylimit(cache); pc++) {
		tmp = *pc;
		*pc = lifo;
		lifo = tmp;
		if (lifo.block == blk) break;
	}
	cache[0] = lifo;

	if (cache[0].block != blk) {
		r = readsectors(0, cache[0].seg, sec, 1 * RATIO);
		cache[0].block = blk;
	}
	raw_copy((u16_t) buf, dseg, 0, cache[0].seg, BLOCK_SIZE);
  }
  if (r != 0) {
	readerr(sec, r);
	reboot();
  }
}

char *readline()
{
/* Read a line including a newline with echoing. */
  char *line;
  size_t i = 0, z;
  int c;

  line = (char *) malloc((z = 20) * sizeof(char));

  do {
	c = getchar();

	if (strchr("\b\177\25\30", c) != NULL) {
		/* Backspace, DEL, ctrl-U, or ctrl-X. */
		do {
			if (i == 0) break;
			printf("\b \b");
			i--;
		} while (c == '\25' || c == '\30');
	} else if (c < ' ' && c != '\n')
		putchar('\7');
	else {
		putchar(c);
		line[i++] = c;
		if (i == z) line = (char *) realloc((void *) line,
					   (z *= 2) * sizeof(char));
	}
  } while (c != '\n');
  line[i] = 0;
  return(line);
}

int sugar(tok)
char *tok;
{
/* Recognize special tokens. */
  return(strchr("=(){};\n", tok[0]) != NULL);
}

char *onetoken(aline, arg)
char **aline;
int arg;
{
/* Returns a string with one token for tokenize.  Arg is true when reading
 * between ( and ).
 */
  char *line = *aline;
  size_t n;
  char *tok;

  /* Skip spaces. */
  while (*line == ' ') line++;

  *aline = line;

  /* Don't do odd junk (nor the terminating 0!). */
  if ((unsigned) *line < ' ' && *line != '\n') return(NULL);

  if (arg) {
	/* Function argument, anything goes except ). */
	int depth = 0;

	while ((unsigned) *line >= ' ') {
		if (*line == '(') depth++;
		if (*line == ')' && --depth < 0) break;
		line++;
	}
	while (line > *aline && line[-1] == ' ') line--;
  } else if (sugar(line)) {
	/* Single character token. */
	line++;
  } else {
	/* Multicharacter token. */
	do
		line++;
	while ((unsigned) *line > ' ' && !sugar(line));
  }
  n = line - *aline;
  tok = (char *) malloc((n + 1) * sizeof(char));
  memcpy((void *) tok, (void *) *aline, n);
  tok[n] = 0;
  if (tok[0] == '\n') tok[0] = ';';	/* ';' same as '\n' */

  *aline = line;
  return(tok);
}

/* Typed commands form strings of tokens. */
token **tokenize(acmds, line, fundef)
token **acmds;			/* Splice tokenized line into this list. */
char *line;			/* Characters to be tokenized. */
int *fundef;			/* Keeps state when forming a function def. */
{
/* Takes a line apart to form tokens.  The tokens are inserted into a command
 * chain at *acmds.  Tokenize returns a reference to where another line could
 * be added.  The fundef variable holds the state tokenize is in when decoding
 * a multiline function definition.  It is nonzero when more must be read.
 * Tokenize looks at spaces as token separators, and recognizes only
 * ';', '=', '(', ')' '{', '}', and '\n' as single character tokens.
 */
  int fd = *fundef;
  char *tok;
  token *newcmd;
  static char funsugar[] = {'(', 0, ')', '{', '}'};

  while ((tok = onetoken(&line, fd == 1)) != NULL) {
	if (fd == 1)
		fd++;		/* Function argument. */
	else if (funsugar[fd] == tok[0]) {
		/* Recognize next token as part of a function def. */
		fd = tok[0] == '}' ? 0 : fd + 1;
	} else if (fd != 0) {
		if (tok[0] == ';' && fd == 3) {
			/* Kill separator between ')' and '{'. */
			free((void *) tok);
			continue;
		}

		/* Syntax error unless between '{' and '}'. */
		if (fd != 4) fd = 0;
	}
	newcmd = (token *) malloc(sizeof(*newcmd));
	newcmd->token = tok;
	newcmd->next = *acmds;
	*acmds = newcmd;
	acmds = &newcmd->next;
  }
  *fundef = fd;
  return(acmds);
}

token *cmds;			/* String of commands to execute. */

char *poptoken()
{
/* Pop one token off the command chain. */
  token *cmd = cmds;
  char *tok = cmd->token;

  cmds = cmd->next;
  free((void *) cmd);

  return(tok);
}

void voidtoken()
/* Remove one token from the command chain. */
{
  free((void *) poptoken());
}

void void_cmds()
{
/* Void the whole list. */
  while (cmds != NULL) voidtoken();
}

void interrupt()
{
/* Clean up after an ESC has been typed. */
  disp_cmds();		/* display the command list */
  while (peekchar() == ESC) (void) getchar();

  /* Delete leftover commands. */
  void_cmds();
}

struct biosdev {
  int device, primary, secondary;
} bootdev, tmpdev;

struct part_entry boot_part;
char dskpars[DSKPARSIZE] =	/* 360K floppy disk parameters (for now). */
{0xDF, 0x02, 25, 2, 9, 0x2A, 0xFF, 0x50, 0xF6, 15, 8};

void migrate()
{
/* Copy boot program to the far end of memory, this must be done asap to
 * put the data area cleanly inside a 64K chunk (no DMA problems).
 */
  u16_t oldseg = cseg;
  u16_t size = (runsize + HCLICK_SIZE - 1) >> HCLICK_SHIFT;
  u16_t memsize = get_low_memsize() * (1024 / HCLICK_SIZE);
  u16_t dma64k = (memsize - 1) & 0xF000;
  u16_t newseg = memsize - size;
  u16_t dst_off, src_off, src_seg;
  vector dskbase;

  /* Check if data segment crosses a 64k boundary. */
  if (newseg + (dseg - cseg) < dma64k) newseg = dma64k - size;

  /* Get some variables into my address space before they get mashed. */
  if (device < 0x80) {
	/* Floppy disk parameters. */
	dst_off = (u16_t) &dskbase;
	src_off = (u16_t) (DSKBASE * sizeof(vector));
	raw_copy(dst_off, dseg, src_off, 0, sizeof(vector));

	dst_off = (u16_t) dskpars;
	src_off = (u16_t) dskbase.offset;
	src_seg = (u16_t) dskbase.segment;
	raw_copy(dst_off, dseg, src_off, src_seg, DSKPARSIZE);
  } else {
	/* Hard disk partition table entry into boot_part. */
	dst_off = (u16_t) &boot_part;
	src_off = (u16_t) rem_part.offset;
	src_seg = rem_part.segment;
	raw_copy(dst_off, dseg, src_off, src_seg, sizeof(boot_part));
  }

  /* Set the new cseg for relocate. */
  cseg = newseg;

  /* Copy code and data in large chunks. */
  do {
	u16_t chunk = size < 0x0FFF ? size : 0x0FFF;

	raw_copy(0, newseg, 0, oldseg, chunk << HCLICK_SHIFT);
	oldseg += chunk;
	newseg += chunk;
	size -= chunk;
  } while (size > 0);

  relocate();			/* Make the copy running. */

  /* Set the parameters for the boot device using global variables
   * device and dskpars.  (This particular call should not fail.) 
   */
  (void) dev_geometry();
}

int get_master(master, table, pos)
char *master;
struct part_entry **table;
u32_t pos;
{
/* Read a master boot sector and its partition table. */
  int r, n;
  struct part_entry *pe, **pt;

  if ((r = readsectors((u16_t) master, dseg, pos, 1)) != 0) return(r);

  pe = (struct part_entry *) (master + PART_TABLE_OFF);
  for (pt = table; pt < table + NR_PARTITIONS; pt++) *pt = pe++;

  /* DOS has the misguided idea that partition tables must be sorted. */
  if (pos != 0) return(0);	/* But only the primary. */

  n = NR_PARTITIONS;
  do {
	for (pt = table; pt < table + NR_PARTITIONS - 1; pt++) {
		if (pt[0]->sysind == NO_PART
		    || (pt[0]->lowsec > pt[1]->lowsec
			&& pt[1]->sysind != NO_PART)) {
			pe = pt[0];
			pt[0] = pt[1];
			pt[1] = pe;
		}
	}
  } while (--n > 0);
  return(0);
}

void initialize()
{
  char master[SECTOR_SIZE];
  struct part_entry *table[NR_PARTITIONS];
  int r, p;
  u32_t masterpos;

  /* Find out what the boot device[A and partition was. */
  bootdev.device = device;
  bootdev.primary = -1;
  bootdev.secondary = -1;

  if (device < 0x80) return;

  /* Get the partition table from the very first sector, and determine
   * the partition we booted from.  Migrate() was so nice to put the
   * partition table entry of the booted partition in boot_part. */

  /* The only thing really needed from the booted partition: */
  lowsec = boot_part.lowsec;

  masterpos = 0;		/* Master bootsector position. */

  for (;;) {
	/* Extract the partition table from the master boot sector. */
	if ((r = get_master(master, table, masterpos)) != 0) {
		readerr(masterpos, r);
		reboot();
	}

	/* See if you can find "lowsec" back. */
	for (p = 0; p < NR_PARTITIONS; p++)
		if (lowsec - table[p]->lowsec < table[p]->size) break;

	if (lowsec == table[p]->lowsec) {	/* Found! */
		if (bootdev.primary < 0)
			bootdev.primary = p;
		else
			bootdev.secondary = p;
		break;
	}

	/* This happens when lightning strikes while booting: */
	if (p == NR_PARTITIONS || bootdev.primary >= 0) {
		printf("Can't find the partition starting at %lu???\n",
		       lowsec);
		reboot();
	}

	/* See if the primary partition is subpartitioned. */
	bootdev.primary = p;
	masterpos = table[p]->lowsec;
  }
}

char null[] = "";

void sfree(s)
char *s;
{
/* Free a non-null string. */
  if (s != NULL && s != null) free((void *) s);
}

char *copystr(s)
char *s;
{
/* Copy a non-null string using malloc. */
  char *c;

  if (*s == 0) return(null);
  c = (char *) malloc((strlen(s) + 1) * sizeof(char));
  strcpy(c, s);
  return(c);
}

int is_default(e)
environment *e;
{
  return(e->flags & E_SPECIAL) && e->defval == NULL;
}

environment **searchenv(name)
char *name;
{
  environment **aenv = &env;

  while (*aenv != NULL && strcmp((*aenv)->name, name) != 0)
	aenv = &(*aenv)->next;

  return(aenv);
}

char *b_value(name)
char *name;
{
/* The value of a variable. */
  environment *e = b_getenv(name);
  return(e == NULL || !(e->flags & E_VAR) ? NULL : e->value);
}

char *b_body(name)
char *name;
/* The value of a function. */
{
  environment *e = b_getenv(name);
  return(e == NULL || !(e->flags & E_FUNCTION) ? NULL : e->value);
}

int b_setenv(flags, name, arg, value)
int flags;
char *name, *arg, *value;
{
/* Change the value of an environment variable.  Returns the flags of the
 * variable if you are not allowed to change it, 0 otherwise.
 */
  environment **aenv, *e;

  if (*(aenv = searchenv(name)) == NULL) {
	e = (environment *) malloc(sizeof(*e));
	e->name = copystr(name);
	e->flags = flags;
	e->defval = NULL;
	e->next = NULL;
	*aenv = e;
  } else {
	e = *aenv;

	/* Don't touch reserved names and don't change special
	 * variables to functions or vv. */
	if (e->flags & E_RESERVED || (e->flags & E_SPECIAL
		  && (e->flags & E_FUNCTION) != (flags & E_FUNCTION)
				      ))
		return(e->flags);

	e->flags = (e->flags & E_STICKY) | flags;
	if (is_default(e))
		e->defval = e->value;
	else
		sfree(e->value);
	sfree(e->arg);
  }
  e->arg = copystr(arg);
  e->value = copystr(value);
  return(0);
}

int b_setvar(flags, name, value)
int flags;
char *name, *value;
/* Set variable or simple function. */
{
  return(b_setenv(flags, name, null, value));
}

void b_unset(name)
char *name;
{
/* Remove a variable from the environment.  A special variable is reset to
 * its default value.
 */
  environment **aenv, *e;

  if ((e = *(aenv = searchenv(name))) == NULL) return;

  if (e->flags & E_SPECIAL) {
	if (e->defval != NULL) {
		sfree(e->arg);
		e->arg = null;
		sfree(e->value);
		e->value = e->defval;
		e->defval = NULL;
	}
  } else {
	sfree(e->name);
	sfree(e->arg);
	sfree(e->value);
	*aenv = e->next;
	free((void *) e);
  }
}


long a2l(a)
char *a;
{
/* Cheap atol(). */
  int sign = 1;
  long l = 0;

  if (*a == '-') {
	sign = -1;
	a++;
  }
  while (between('0', *a, '9')) l = l * 10 + (*a++ - '0');

  return(sign * l);
}

char *ul2a(n)
u32_t n;
{
/* Transform a long number to ascii digits. */
  static char num[3 * sizeof(n)];
  char *pn = arraylimit(num) - 1;

  do {
	*--pn = (n % 10) + '0';
  } while ((n /= 10) > 0);
  return(pn);
}

char *u2a(n1)
int n1;
{
/* Transform a short number to ascii digits. */
  u16_t n;

  n = (u16_t) n1;
  return(ul2a((u32_t) n));
}

void get_parameters()
{
  char params[SECTOR_SIZE + 1];
  token **acmds;
  int vid, r, fundef = 0;
  static char *vid_type[] = {"mda", "cga", "ega", "ega", "vga", "vga"};

  /* Variables that Minix needs: */
  b_setvar(E_SPECIAL | E_VAR | E_DEV, "rootdev", "ram");
  b_setvar(E_SPECIAL | E_VAR | E_DEV, "ramimagedev", "bootdev");
  b_setvar(E_SPECIAL | E_VAR, "ramsize", "0");
  b_setvar(E_SPECIAL | E_VAR, "keyboard", "standard");
  b_setvar(E_SPECIAL | E_VAR, "processor", u2a(get_processor()));
  b_setvar(E_SPECIAL | E_VAR, "memsize", u2a(get_low_memsize()));
  b_setvar(E_SPECIAL | E_VAR, "emssize", u2a(get_ext_memsize()));
  vid = get_video();
  b_setvar(E_SPECIAL | E_VAR, "chrome", (vid & 1) ? "color" : "mono");
  b_setvar(E_SPECIAL | E_VAR, "video", vid_type[vid]);

  /* Variables boot needs: */
  b_setvar(E_SPECIAL | E_VAR, "image", "minix");
  b_setvar(E_SPECIAL | E_FUNCTION, "main", "menu");

  /* Default menu function: */
  b_setenv(E_RESERVED | E_FUNCTION, "\1", "=,Start Minix", "boot");

  /* Reserved names: */
  b_setvar(E_RESERVED, "scancode", null);
  b_setvar(E_RESERVED, "boot", null);
  b_setvar(E_RESERVED, "menu", null);
  b_setvar(E_RESERVED, "set", null);
  b_setvar(E_RESERVED, "unset", null);
  b_setvar(E_RESERVED, "save", null);
  b_setvar(E_RESERVED, "ls", null);
  b_setvar(E_RESERVED, "echo", null);
  b_setvar(E_RESERVED, "trap", null);

  /* Tokenize bootparams sector. */
  if ((r = readsectors((u16_t) params, dseg, lowsec + PARAMSEC, 1)) != 0)
	readerr(lowsec + PARAMSEC, r);
  else {
	params[SECTOR_SIZE] = 0;
	acmds = tokenize(&cmds, params, &fundef);

	/* Reboot code may have new parameters. */
	if (device >= 0x80 && boot_part.sysind == NO_PART) {
		raw_copy((u16_t) params, dseg, (u16_t) (boot_part.size & 0x0F),
				 (u16_t) (boot_part.size >> 4), SECTOR_SIZE);
		acmds = tokenize(&cmds, params, &fundef);
	}

	/* Stuff the default action into the command chain. */
	(void) tokenize(acmds, ":;main", &fundef);
  }
}

void addparm(ap, n)
char **ap, *n;
{
  char *p = *ap;
  while (*n != 0 && *p != 0) *p++ = *n++;
  *ap = p;
}

void save_parameters()
{
/* Save nondefault environment variables to the bootparams sector. */
  environment *e;
  char params[SECTOR_SIZE + 1];
  char *p = params;
  int r;

  /* Default filling: */
  memset((void *) params, '\n', (size_t) SECTOR_SIZE);

  /* Don't touch the 0! */
  params[SECTOR_SIZE] = 0;

  for (e = env; e != NULL; e = e->next) {
	if (e->flags & E_RESERVED || is_default(e)) continue;

	addparm(&p, e->name);
	if (e->flags & E_FUNCTION) {
		addparm(&p, "(");
		addparm(&p, e->arg);
		addparm(&p, "){");
	} else {
		addparm(&p, (e->flags & (E_DEV | E_SPECIAL)) != E_DEV
			? "=" : "=d ");
	}
	addparm(&p, e->value);
	if (e->flags & E_FUNCTION) addparm(&p, "}");
	if (*p == 0) {
		printf("The environment is too big\n");
		return;
	}
	*p++ = '\n';
  }

  /* Save the parameters on disk. */
  if ((r = writesectors((u16_t) params, dseg, lowsec + PARAMSEC, 1)) != 0) {
	writerr(lowsec + PARAMSEC, r);
	printf("Can't save environment\n");
  }
}

void show_env()
{
/* Show the environment settings. */
  environment *e;

  for (e = env; e != NULL; e = e->next) {
	if (e->flags & E_RESERVED) continue;

#ifndef EXTENDED_LIST
/* Suppress main and delay unless EXTENDED_LIST is defined. */
	if (strcmp(e->name, "main") == 0) continue;
	if (strcmp(e->name, "delay") == 0) continue;
#endif

	if (e->flags & E_FUNCTION) {
		printf("%s(%s) {%s}", e->name, e->arg, e->value);
	} else {
		printf("%s = ", e->name);
		if (is_default(e)) putchar('(');
		printf("%s", e->value);
		if (is_default(e)) putchar(')');
	}
	putchar('\n');
  }
}

int numprefix(s, ps)
char *s, **ps;
{
/* True iff s is a string of digits.  *ps will be set to the first nondigit
 * if non-NULL, otherwise the string should end.
 */
  if (!between('0', *s, '9')) return(0);

  do
	s++;
  while (between('0', *s, '9'));

  if (ps == NULL) return(*s == 0);

  *ps = s;
  return(1);
}

int numeric(s)
char *s;
{
  return(numprefix(s, (char **) NULL));
}

dev_t name2dev(name)
char *name;
{
/* Translate, say, /dev/hd3 to a device number.  If the name can't be
 * found on the boot device, then do some guesswork.  The global structure
 * "tmpdev" will be filled in based on the name, so that "boot hd6" knows
 * what device to boot without interpreting device numbers.
 */
  dev_t dev = -1;
  ino_t ino;
  struct stat st;
  char *n, *s;
  static char fdN[] = "fdN";
  static char hdNX[] = "hdNNX";
  static char sub[] = "a";

  tmpdev.primary = tmpdev.secondary = -1;

  /* The special name "bootdev" must be translated to the boot device. */
  if (strcmp(name, "bootdev") == 0) {
	if (device < 0x80) {
		/* Floppy disk. */
		strcpy(fdN + 2, u2a(device));
		name = fdN;
	} else {
		/* Hard disk */
		strcpy(hdNX + 2, u2a((device - 0x80) * (1 + NR_PARTITIONS)
				     + 1 + bootdev.primary));
		if (bootdev.secondary >= 0) {
			sub[0] = 'a' + bootdev.secondary;
			strcat(hdNX, sub);
		}
		name = hdNX;
	}
  }

  /* Now translate fd0, hd6, etc. the BIOS way. */
  n = name;
  if (strncmp(n, "/dev/", 5) == 0) n += 5;

  if (n[0] == 'f' && n[1] == 'd' && numeric(n + 2))
	tmpdev.device = a2l(n + 2);
  else if (n[0] == 'h' && n[1] == 'd' && numprefix(n + 2, &s)) {
	tmpdev.primary = a2l(n + 2);
	tmpdev.device = 0x80 + tmpdev.primary / (1 + NR_PARTITIONS);
	tmpdev.primary = (tmpdev.primary % (1 + NR_PARTITIONS)) - 1;
	if (*s == 0)
		 /* Not a secondary */ ;
	else if (tmpdev.primary >= 0 && between('a', *s, 'd') && s[1] == 0)
		tmpdev.secondary = *s - 'a';
	else
		tmpdev.device = -1;
  } else
	tmpdev.device = -1;

  /* Look the name up on the boot device for the UNIX device number. */
  if (fsok) {
	/* The current working directory is "/dev". */
	ino = name[0] == '/' ? ROOT_INO : r_lookup(ROOT_INO, "dev");

	if (ino != 0) ino = r_lookup(ino, name);

	if (ino != 0) {
		/* Name has been found, extract the device number. */
		r_stat(ino, &st);
		if (!S_ISBLK(st.st_mode)) {
			printf("%s is not a block device\n");
			errno = 0;
			return((dev_t) -1);
		}
		dev = st.st_rdev;
	}
  }
  if (dev == -1 && tmpdev.device >= 0) {
	/* The name can't be found on the boot device, do guesswork. */
	if (tmpdev.device < 0x80) 
		dev = DEV_FD0 + tmpdev.device;
	else if (tmpdev.secondary < 0)
		dev = DEV_HD0 + (tmpdev.device - 0x80) * (1 + NR_PARTITIONS)
			      + (1 + tmpdev.primary);
	else
		dev = DEV_HD1a + ((tmpdev.device - 0x80) * NR_PARTITIONS
			  + tmpdev.primary) * NR_PARTITIONS + tmpdev.secondary;
  }

  /* Don't forget the RAM device. */
  if (dev == -1 && strcmp(n, "ram") == 0) dev = DEV_RAM;

  if (dev == -1) {
	printf("Can't recognize %s as a device\n", name);
	errno = 0;
  }
  return(dev);
}

int exec_bootstrap()
{
/* Load boot sector from the disk or floppy described by tmpdev and execute it.
 * The floppy parameters may not be right for the floppy we want to read, but
 * reading sector 0 seems to be no problem.
 */
  int r;
  char master[SECTOR_SIZE];
  struct part_entry *table[NR_PARTITIONS], dummy, *active = &dummy;
  u32_t masterpos;

  device = tmpdev.device;
  if (!dev_geometry()) return(B_NODEV);

  active->lowsec = 0;

  /* Select a partition table entry. */
  while (tmpdev.primary >= 0) {
	masterpos = active->lowsec;

	if ((r = get_master(master, table, masterpos)) != 0) return(r);

	active = table[tmpdev.primary];

	/* How does one check a partition table entry? */
	if (active->sysind == NO_PART) return(B_NOSIG);

	tmpdev.primary = tmpdev.secondary;
	tmpdev.secondary = -1;
  }

  /* Read the boot sector. */
  if ((r = readsectors(0, BOOTSEG, active->lowsec, 1)) != 0) return(r);

  /* Check signature word. */
  if (get_word(SIGNATPOS, BOOTSEG) != SIGNATURE) return(B_NOSIG);

  bootstrap(device, (u16_t) active, dseg);
}

void boot_device(devname)
char *devname;
{
  dev_t dev = name2dev(devname);
  int r;

  if (tmpdev.device < 0) {
	if (dev != -1) printf("Can't boot from %s\n", devname);
	return;
  }
  switch (r = exec_bootstrap()) {
      case B_NODEV:
	printf("%s: device not present\n", devname);
	break;
      case B_NOSIG:
	printf("%s is not bootable\n", devname);
	break;
      default:
	printf("Can't boot %s: %s\n", devname, bios_err(r));
  }
  device = bootdev.device;
  (void) dev_geometry();	/* Restore boot device setting. */
}

void ls(dir)
char *dir;
{
  ino_t ino;
  struct stat st;
  char name[NAME_MAX + 1];

  if (!fsok) return;

  if ((ino = r_lookup(ROOT_INO, dir)) == 0
      || (r_stat(ino, &st), r_readdir(name)) == -1
	) {
	printf("ls: %s: %s\n", dir, unix_err(errno));
	return;
  }
  (void) r_readdir(name);	/* Skip ".." too. */

  while ((ino = r_readdir(name)) != 0) printf("%s/%s\n", dir, name);
}

char *Thandler;
u32_t Tbase, Tcount;

void unschedule()
{
/* Invalidate a waiting command. */
  if (Thandler != NULL) {
	free((void *) Thandler);
	Thandler = NULL;
  }
}

void schedule(msec, cmd)
long msec;
char *cmd;
{
/* Schedule command at a certain time from now. */
  unschedule();
  Thandler = cmd;
  Tbase = get_tick();
  Tcount = (msec + MSEC_PER_TICK - 1) / MSEC_PER_TICK;
}

int expired()
{
/* Check if the timer expired.  If so prepend the scheduled command to
 * the command chain and return 1.
 */
  int fundef = 0;

  if (Thandler == NULL || (get_tick() - Tbase) < Tcount) return(0);
  (void) tokenize(tokenize(&cmds, Thandler, &fundef), ";", &fundef);
  unschedule();
  return(1);
}

int delay(msec)
char *msec;
{
/* Delay for a given time.  Returns true iff delay was not interrupted.
 * Make sure get_tick is not called for nonpositive msec, because get_tick
 * may do funny things on the original IBM PC (not the XT!).
 * If msec happens to be the string "swap" then wait till the user hits
 * return after changing diskettes.
 */
  int swap = 0;
  u32_t base, count;

  if (strcmp(msec, "swap") == 0) {
	swap = 1;
	count = 0;
	printf("\nInsert the root diskette then hit RETURN\n");
  } else if ((count = a2l(msec)) > 0) {
	count /= MSEC_PER_TICK;
	base = get_tick();
  }
  do {
	switch (peekchar()) {
	    case 0:
		break;
	    case ESC:
		interrupt();
		return(0);
	    case '\n':
		swap = 0;
	    default:	(void) getchar();
	}
  } while (!expired()
	 && (swap || (count > 0 && (get_tick() - base) < count))
	);
  return(1);
}

enum whatfun menufun(e)
environment *e;
{
  if (!(e->flags & E_FUNCTION) || e->arg[0] == 0) return(NOFUN);
  if (e->arg[1] != ',') return(SELECT);
  if (e->flags & E_RESERVED) return(DEFFUN);
  return(USERFUN);
}

void menu()
{
/* By default:  Show a simple menu.
 * Multiple kernels/images:  Show extra selection options.
 * User defined function:  Kill the defaults and show these.
 * Wait for a keypress and execute the given function.
 */
  int fundef = 0, c, def = 1;
  char *choice = NULL;
  environment *e;

  /* Just a default menu? */
  for (e = env; e != NULL; e = e->next)
	if (menufun(e) == USERFUN) def = 0;

  printf("\nHit a key as follows:\n\n");

  /* Show the choices. */
  for (e = env; e != NULL; e = e->next) {
	switch (menufun(e)) {
	    case DEFFUN:
		if (!def) break;
	    case USERFUN:
		printf("    %c  %s\n", e->arg[0], e->arg + 2);
		break;
	    case SELECT:
		printf("    %c  Select %s kernel\n", e->arg[0], e->name);
	}
  }

  /* Wait for a keypress. */
  do {
	while (peekchar() == 0)
		if (expired()) return;

	if ((c = getchar()) == ESC) {
		interrupt();
		return;
	}
	for (e = env; e != NULL; e = e->next) {
		switch (menufun(e)) {
		    case DEFFUN:
			if (!def) break;
		    case USERFUN:
		    case SELECT:
			if (c == e->arg[0]) choice = e->value;
		}
	}
  } while (choice == NULL);

  /* Execute the chosen function. */
  printf("%c\n", c);
  (void) tokenize(&cmds, choice, &fundef);
}

void execute()
{
/* Get one command from the command chain and execute it.  Legal commands are
 * listed at the end.
 */
  token *second, *third, *fourth, *fifth, *sep;
  char *name = cmds->token;
  size_t n = 0;

  /* There must be a separator lurking somewhere. */
  for (sep = cmds; sep != NULL && sep->token[0] != ';'; sep = sep->next) n++;

  if ((second = cmds->next) != NULL
      && (third = second->next) != NULL
      && (fourth = third->next) != NULL)
	fifth = fourth->next;

  /* Null command? */
  if (n == 0) {
	voidtoken();
	return;
  } else
	/* Name = [device] value? */
	if ((n == 3 || n == 4) && !sugar(name)
	    && second->token[0] == '='
	    && !sugar(third->token)
	    && (n == 3 || (n == 4 && third->token[0] == 'd'
			   && !sugar(fourth->token)
			   ))) {
	char *value = third->token;
	int flags = E_VAR;

	if (n == 4) {
		value = fourth->token;
		flags |= E_DEV;
	}
	if ((flags = b_setvar(flags, name, value)) != 0) {
		printf("%s is a %s\n", name,
		       flags & E_RESERVED ? "reserved word" :
		       "special function");
		void_cmds();
	} else
		while (cmds != sep) voidtoken();
	return;
  } else
	/* Name '(' arg ')' '{' ... '}'? */
	if (n >= 5
	    && !sugar(name)
	    && second->token[0] == '('
	    && fourth->token[0] == ')'
	    && fifth->token[0] == '{'
	) {
	token *fun = fifth->next;
	int ok = 1, flags;
	char *body;
	size_t len = 1;

	sep = fun;
	while (sep != NULL && sep->token[0] != '}') {
		len += strlen(sep->token) + 1;
		sep = sep->next;
	}
	if (sep == NULL || (sep = sep->next) == NULL
	    || sep->token[0] != ';'
		)
		ok = 0;

	if (ok) {
		body = (char *) malloc(len * sizeof(char));
		*body = 0;

		while (fun->token[0] != '}') {
			strcat(body, fun->token);
			if (!sugar(fun->token) && !sugar(fun->next->token)
				)
				strcat(body, " ");
			fun = fun->next;
		}

		if ((flags = b_setenv(E_FUNCTION, name,
				      third->token, body)) != 0) {
			printf("%s is a %s\n", name,
			       flags & E_RESERVED ? "reserved word" :
			       "special variable");
			void_cmds();
		} else
			while (cmds != sep) voidtoken();
		free((void *) body);
		return;
	}
  } else
	/* Command coming up, check if ESC typed. */
  if (peekchar() == ESC) {
	interrupt();
	return;
  } else
	/* Unset name ..., echo word ...? */
	if (n >= 1 && (
		       strcmp(name, "unset") == 0
		       || strcmp(name, "echo") == 0
		       )) {
	int cmd = name[0];
	char *arg = poptoken();

	for (;;) {
		free((void *) arg);
		if (cmds == sep) break;
		arg = poptoken();
		if (cmd == 'u')
			b_unset(arg);
		else {
			printf("%s", arg);
			if (cmds != sep) putchar(' ');
		}
	}
	if (cmd == 'e') putchar('\n');
	return;
  } else
	/* Boot device, ls dir, delay msec? */
	if (n == 2 && (strcmp(name, "boot") == 0 || strcmp(name, "delay") == 0
					       || strcmp(name, "ls") == 0 )) {
	int cmd = name[0];
	char *arg;

	voidtoken();
	arg = poptoken();
	switch (cmd) {
	    case 'b':	boot_device(arg);	break;
	    case 'd':	(void) delay(arg);	break;
	    case 'l':	ls(arg);
	}
	free((void *) arg);
	return;
  } else
	/* Trap msec command? */
  if (n == 3 && strcmp(name, "trap") == 0 && numeric(second->token)) {
	long msec = a2l(second->token);

	voidtoken();
	voidtoken();
	schedule(msec, poptoken());
	return;
  } else
	/* Simple command. */
  if (n == 1) {
	char *cmd = poptoken();
	char *body;
	int fundef = 0;
	int ok = 0;

	if (strcmp(cmd, "boot") == 0) {
		minix();
		ok = 1;
	}
	if (strcmp(cmd, "delay") == 0) {
		(void) delay("500");
		ok = 1;
	}
	if (strcmp(cmd, "ls") == 0) {
		ls("");
		ok = 1;
	}
	if (strcmp(cmd, "menu") == 0) {
		menu();
		ok = 1;
	}
	if (strcmp(cmd, "save") == 0) {
		save_parameters();
		ok = 1;
	}
	if (strcmp(cmd, "set") == 0) {
		show_env();
		ok = 1;
	}

	/* Command to check bootparams: */
	if (strcmp(cmd, ":") == 0) ok = 1;

	/* User defined function. */
	if (!ok && (body = b_body(cmd)) != NULL) {
		(void) tokenize(&cmds, body, &fundef);
		ok = 1;
	}
	if (!ok) printf("%s: unknown function", cmd);
	free((void *) cmd);
	if (ok) return;
  } else {
	/* Syntax error. */
	printf("Can't parse:");
	while (cmds != sep) {
		printf(" %s", cmds->token);
		voidtoken();
	}
  }
  disp_cmds();
  void_cmds();
}

void monitor()
/* Read one or more lines and tokenize them. */
{
  char *line;
  int fundef = 0;
  token **acmds = &cmds;

  unschedule();			/* Kill a trap. */

  do {
	putchar(fundef == 0 ? '>' : '+');
	line = readline();
	acmds = tokenize(acmds, line, &fundef);
	free((void *) line);
  } while (fundef != 0);
}

void boot()
{
/* Load Minix and start it, among other things. */
  int color;

  /* Print greeting message.  The copyright message is not yet
   * displayed, because this boot program need not necessarily start
   * Minix. */
  color = get_video() & 1;		/* true if color */
  reset_video(color);			/* clear the screen */
  printf("\nMINIX boot monitor\n");
  printf("\nPress ESC to enter the monitor to change boot parameters\n");

  /* Relocate program to end of memory. */
  migrate();

  /* Initialize tables. */
  initialize();

  /* Block cache. */
  init_cache();

  /* Get environment variables from the parameter sector. */
  get_parameters();

  /* Read and check the superblock. */
  fsok = r_super() != 0;

  while (1) {
	/* While there are commands, execute them! */
	while (cmds != NULL) {
		execute();
		(void) expired();
	}

	/* The "monitor" is just a "read one command" thing. */
	monitor();
  }
}

void disp_cmds()
{
  /* Display commands. */
  printf("\n\n");
  printf("\nCommand summary:\n");
  printf("    name = value       - Set environment variable\n");
  printf("    boot [device]      - Boot Minix [e.g. boot  or  boot hd3]\n");
  printf("    menu               - Return to previous menu\n");
  printf("    save               - Save environment on disk\n");
  printf("    set                - Show environment variables\n");
  printf("    help               - Redisplay this command summary\n");

/* Only show all the rest of this stuff if there is genuine interest. */
#ifdef EXTENDED_LIST
  printf("    name(arg) { ... }  - Define function\n");
  printf("    name               - Call function\n");
  printf("    ls [directory]     - List contents of directory\n");
  printf("    trap msec command  - Schedule command\n");
  printf("    unset name ...     - Unset environment variables\n");
  printf("    delay [msec]       - Delay (500 msec default)\n");
  printf("    echo word ...      - Print these words\n");
#endif

#ifndef EXTENDED_LIST
/* Advanced users know this stuff, and besides, there is no room for it. */
  printf("\n\nValid environmental variables that can be set. Defaults in ().\n");
  printf("    rootdev [e.g. hd3] - Root device or (ram)\n");
  printf("    ramimagedev        - Where to get RAM image (boot device)\n");
  printf("    ramsize            - Size of RAM disk when not used for root (0)\n");
  printf("    keyboard           - Keyboard type (olivetti, us, dutch) (standard)\n");
  printf("    processor          - CPU: 86, 186, 286, 386, 486 (automatic)\n");
  printf("    memsize            - Kilobytes of conventional memory (automatic)\n");
  printf("    emsize             - Kilobytes of extended memory (automatic)\n");
  printf("    chrome             - Monitor type: mono or color (automatic)\n");
  printf("    video              - Display: mda, cga, ega, vga (automatic)\n");
  printf("    image              - FD: start:length;  HD: image file (minix)\n");
  printf("\n");
#endif
}
