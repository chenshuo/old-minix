#include <amoeba.h>

uniqport(p)
port *p;
{
	register char *c;
	register short r;
	register n;
	static been_here;

	if (!been_here) {
		long t, time();

		time(&t);
		srand(getpid() + (int)((t>>16) + t));
		been_here = 1;
	}
	do {
		c = (char *) p;
		n = PORTSIZE / 2;
		do {
			r = rand();
			*c++ ^= r >> 8;
			*c++ ^= r & 0377;
		} while (--n);
	} while (NullPort(p));
}
