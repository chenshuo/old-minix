#include <curses.h>
#include "curspriv.h"

void raw()
{
  _cursvar.rawmode = TRUE;
  _tty.sg_flags |= RAW;
  stty(0, &_tty);
}				/* raw */

void noraw()
{
  _cursvar.rawmode = FALSE;
  _tty.sg_flags &= ~RAW;
  stty(0, &_tty);
}				/* noraw */

void echo()
{
  _cursvar.echoit = TRUE;
  _tty.sg_flags |= ECHO;
  stty(0, &_tty);
}

void noecho()
{
  _cursvar.echoit = FALSE;
  _tty.sg_flags &= ~ECHO;
  stty(0, &_tty);
}

void nl()
{
  _tty.sg_flags |= CRMOD;
  NONL = FALSE;
  stty(0, &_tty);
}				/* nl */

void nonl()
{
  _tty.sg_flags &= ~CRMOD;
  NONL = TRUE;
  stty(0, &_tty);
}				/* nonl */

void cbreak()
{
  _tty.sg_flags |= CBREAK;
  _cursvar.rawmode = TRUE;
  stty(0, &_tty);
}				/* cbreak */

void nocbreak()
{
  _tty.sg_flags &= ~CBREAK;
  _cursvar.rawmode = FALSE;
  stty(0, &_tty);
}				/* nocbreak */
