/*	loadkeys - load national keyboard map		Author: Marcus Hampel
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <minix/keymap.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if __minix_vmd
#define KBD_DEVICE	"/dev/kbd"
#else
#define KBD_DEVICE	"stdin"
#endif

u16_t keymap[NR_SCAN_CODES * MAP_COLS];
u8_t comprmap[4 + NR_SCAN_CODES * MAP_COLS * 9/8 * 2 + 1];


void tell(char *s)

{
  write(2, s, strlen(s));
}


void report(char *say)

{
  int err = errno;
  tell("loadkeys: ");
  tell(say);
  tell(": ");
  tell(strerror(err));
  tell("\n");
}


void usage(void)

{
  tell("Usage: loadkeys mapfile\n");
  exit(1);
}


int main(int argc, char *argv[])

{
  u8_t *cm;
  u16_t *km;
  int fd, n, fb;

  if (argc != 2)
	usage();

  if ((fd = open(argv[1], O_RDONLY)) < 0) {
	report(argv[1]);
	exit(1);
  }

  if ((n = read(fd, comprmap, sizeof(comprmap))) <
  					4 + NR_SCAN_CODES * MAP_COLS * 9/8) {
	if (n < 0) {
		report(argv[1]);
	} else {
		tell("loadkeys: ");
		tell(argv[1]);
		tell(": too short\n");
	}
	exit(1);
  }

  if (memcmp(comprmap, KEY_MAGIC, 4) != 0) {
	tell("loadkeys: ");
	tell(argv[1]);
	tell(": not a keymap file\n");
	exit(1);
  }
  close(fd);

  /* Decompress the keymap data. */
  cm = comprmap + 4;
  n = 8;
  for (km = keymap; km < keymap + NR_SCAN_CODES * MAP_COLS; km++) {
	if (n == 8) {
		/* Need a new flag byte. */
		fb = *cm++;
		n = 0;
	}
	*km = *cm++;			/* Low byte. */
	if (fb & (1 << n)) {
		*km |= (*cm++ << 8);	/* One of the few special keys. */
	}
	n++;
  }

#if __minix_vmd
  if ((fd = open(KBD_DEVICE,O_WRONLY)) < 0) {
	report(KBD_DEVICE);
	exit(1);
  }
#else
  fd = 0;
#endif

  if (ioctl(fd, KIOCSMAP, keymap) < 0) {
	report(KBD_DEVICE);
	exit(1);
  }
  exit(0);
}
