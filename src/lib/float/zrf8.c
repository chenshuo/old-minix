/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header: zrf8.c,v 1.3 88/07/25 10:46:04 ceriel Exp $ */

/*
	return a zero double (ZRF 8)
*/

#include "FP_types.h"

void
zrf8(z)
DOUBLE	*z;
{

	z->d[0] = 0L;
	z->d[1] = 0L;
}
