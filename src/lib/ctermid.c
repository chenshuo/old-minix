/*  ctermid(3)
 *
 *  Author: Terrence Holm          Aug. 1988
 *
 *
 *  Ctermid(3) returns a pointer to a string naming the controlling
 *  terminal. If <name_space> is NULL then local static storage
 *  is used, otherwise <name_space> must point to storage of at
 *  least L_ctermid characters.
 *
 *  Returns a pointer to "/dev/tty".
 */

#include  <stdio.h>

#ifndef L_ctermid
#define L_ctermid  9
#endif


char *ctermid( name_space )
  char *name_space;

  {
  static char termid[ L_ctermid ];

  if ( name_space == NULL )
    name_space = termid;

  strcpy( name_space, "/dev/tty" );

  return( name_space );
  }
