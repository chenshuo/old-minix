/*	screendump 1.0 - dump the contents of the console
 *							Author: Kees J. Bot
 *								16 Dec 1994
 */
#define nil 0
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define MONO_BASE	0xB0000L	/* Screen memory in monochrome mode. */
#define COLOR_BASE	0xB8000L	/* ... colour mode. */

#define COLUMNS		80
#define LINES		25

char MEMORY[] =		"/dev/mem";	/* Memory device to read screen. */

#if !__minix_vmd
#define sysenv(var, val, len)	((errno= ENOSYS), -1)
#endif

long video_base(void)
/* Is it monochrome or colour? */
{
	char chrome[20];

	if (sysenv("chrome", chrome, sizeof(chrome)) == 0
					&& strcmp(chrome, "mono") == 0) {
		return MONO_BASE;
	} else {
		return COLOR_BASE;
	}
}

void tell(const char *message)
{
	write(2, message, strlen(message));
}

void fatal(const char *label)
{
	const char *err= strerror(errno);

	tell("screendump: ");
	tell(label);
	tell(": ");
	tell(err);
	tell("\n");
	exit(1);
}

void main(int argc, char **argv)
{
	int mem;
	unsigned char screen[COLUMNS * LINES * 2];
	unsigned char *ps;
	long base;
	int row;

	base= video_base();
	if (argc == 2 && strcmp(argv[1], "-c") == 0) base= COLOR_BASE;
	else
	if (argc == 2 && strcmp(argv[1], "-m") == 0) base= MONO_BASE;
	else
	if (argc != 1) {
		tell("Usage: screendump [-cm]\n");
		exit(1);
	}

	/* Open the memory device and read screen memory. */
	if ((mem= open(MEMORY, O_RDONLY)) < 0) fatal(MEMORY);
	if (lseek(mem, base, SEEK_SET) == -1) fatal(MEMORY);

	switch (read(mem, screen, sizeof(screen))) {
	case -1:
		fatal(MEMORY);
	default:
		tell("screendump: can't obtain screen dump: short read\n");
		exit(1);
	case sizeof(screen):
		/* Fine */;
	}

	/* Print the contents of the screen line by line.  Omit trailing
	 * blanks.  Note that screen memory consists of pairs of characters
	 * and attribute bytes.
	 */
	ps= screen;
	for (row= 0; row < LINES; row++) {
		char line[COLUMNS + 1];
		char *pl= line;
		int column;
		int blanks= 0;

		for (column= 0; column < COLUMNS; column++) {
			if (*ps <= ' ') {
				/* Skip trailing junk. */
				blanks++;
			} else {
				/* Reinsert blanks and add a character. */
				while (blanks > 0) { *pl++= ' '; blanks--; }
				*pl++= *ps;
			}
			/* Skip character and attribute byte. */
			ps+= 2;
		}
		*pl++= '\n';
		if (write(1, line, pl - line) < 0) fatal("stdout");
	}
	exit(0);
}
