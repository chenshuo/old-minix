#include <string.h>
#include <curses.h>
#include "curspriv.h"

static char printscanbuf[513];	/* buffer used during I/O */

#ifdef _ANSI
/****************************************************************/
/* Wprintw(win,fmt,args) does a printf() in window 'win'.	*/
/****************************************************************/
int wprintw(WINDOW *win, char *fmt, va_list args, ...)
{
  va_start(args, fmt);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(win, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}

/****************************************************************/
/* Printw(fmt,args) does a printf() in stdscr.			*/
/****************************************************************/
int printw(char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(stdscr, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}				/* printw */

/****************************************************************/
/* Mvprintw(fmt,args) moves the stdscr cursor to a new posi-	*/
/* Tion, then does a printf() in stdscr.			*/
/****************************************************************/
int mvprintw(int y, int x, char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if (wmove(stdscr, y, x) == ERR) return(ERR);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(stdscr, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}

/****************************************************************/
/* Mvwprintw(win,fmt,args) moves the window 'win's cursor to	*/
/* A new position, then does a printf() in window 'win'.	*/
/****************************************************************/
int mvwprintw(WINDOW *win, int y, int x, char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if (wmove(win, y, x) == ERR) return(ERR);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(win, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}				/* mvwprintw */
#else
/****************************************************************/
/* Wprintw(win,fmt,args) does a printf() in window 'win'.	*/
/****************************************************************/
int wprintw(win, fmt, args)
WINDOW *win; 
char *fmt;
va_list args;
{
  va_start(args, fmt);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(win, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}

/****************************************************************/
/* Printw(fmt,args) does a printf() in stdscr.			*/
/****************************************************************/
int printw(fmt)
char *fmt;
{
  va_list args;

  va_start(args, fmt);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(stdscr, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}				/* printw */

/****************************************************************/
/* Mvprintw(fmt,args) moves the stdscr cursor to a new posi-	*/
/* Tion, then does a printf() in stdscr.			*/
/****************************************************************/
int mvprintw(y, x, fmt)
int y;
int x;
char *fmt;
{
  va_list args;

  va_start(args, fmt);
  if (wmove(stdscr, y, x) == ERR) return(ERR);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(stdscr, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}

/****************************************************************/
/* Mvwprintw(win,fmt,args) moves the window 'win's cursor to	*/
/* A new position, then does a printf() in window 'win'.	*/
/****************************************************************/
int mvwprintw(win, y, x, fmt)
WINDOW *win;
int y;
int x;
char *fmt;
{
  va_list args;

  va_start(args, fmt);
  if (wmove(win, y, x) == ERR) return(ERR);
  vsprintf(printscanbuf, fmt, args);
  if (waddstr(win, printscanbuf) == ERR) return(ERR);
  return(strlen(printscanbuf));
}				/* mvwprintw */
#endif

/****************************************************************/
/* Wscanw(win,fmt,args) gets a string via window 'win', then	*/
/* Scans the string using format 'fmt' to extract the values	*/
/* And put them in the variables pointed to the arguments.	*/
/****************************************************************/
int wscanw(win, fmt, A1, A2, A3, A4, A5)
WINDOW *win;
char *fmt;
char *A1, A2, A3, A4, A5;	/* really pointers */
{
  wrefresh(win);		/* set cursor */
  if (wgetstr(win, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(sscanf(printscanbuf, fmt, A1, A2, A3, A4, A5));
}				/* wscanw */

/****************************************************************/
/* Scanw(fmt,args) gets a string via stdscr, then scans the	*/
/* String using format 'fmt' to extract the values and put them	*/
/* In the variables pointed to the arguments.			*/
/****************************************************************/
int scanw(fmt, A1, A2, A3, A4, A5)
char *fmt;
char *A1, A2, A3, A4, A5;	/* really pointers */
{
  wrefresh(stdscr);		/* set cursor */
  if (wgetstr(stdscr, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(sscanf(printscanbuf, fmt, A1, A2, A3, A4, A5));
}				/* scanw */

/****************************************************************/
/* Mvscanw(y,x,fmt,args) moves stdscr's cursor to a new posi-	*/
/* Tion, then gets a string via stdscr and scans the string	*/
/* Using format 'fmt' to extract the values and put them in the	*/
/* Variables pointed to the arguments.				*/
/****************************************************************/
int mvscanw(y, x, fmt, A1, A2, A3, A4, A5)
int y;
int x;
char *fmt;
char *A1, A2, A3, A4, A5;	/* really pointers */
{
  if (wmove(stdscr, y, x) == ERR) return(ERR);
  wrefresh(stdscr);		/* set cursor */
  if (wgetstr(stdscr, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(sscanf(printscanbuf, fmt, A1, A2, A3, A4, A5));
}				/* mvscanw */

/****************************************************************/
/* Mvwscanw(win,y,x,fmt,args) moves window 'win's cursor to a	*/
/* New position, then gets a string via 'win' and scans the	*/
/* String using format 'fmt' to extract the values and put them	*/
/* In the variables pointed to the arguments.			*/
/****************************************************************/
int mvwscanw(win, y, x, fmt, A1, A2, A3, A4, A5)
WINDOW *win;
int y;
int x;
char *fmt;
char *A1, A2, A3, A4, A5;	/* really pointers */
{
  if (wmove(win, y, x) == ERR) return(ERR);
  wrefresh(win);		/* set cursor */
  if (wgetstr(win, printscanbuf) == ERR)	/* get string */
	return(ERR);
  return(sscanf(printscanbuf, fmt, A1, A2, A3, A4, A5));
}				/* mvwscanw */
