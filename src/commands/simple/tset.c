/* tset - set the TERM variable		Author: Terrence Holm */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <unistd.h>
#include <stdio.h>

#define  LINE_LENGTH  40	/* Max length in /etc/ttytype	 */
#define  TC_BUFFER  1024	/* Size of termcap(3) buffer	 */

/****************************************************************/
/*								*/
/*	eval `tset  [ device_type ]`				*/
/*								*/
/*	"device_type" is the new name for $TERM. If no		*/
/*	type is supplied then /etc/ttytype is scanned for	*/
/*	the current port.					*/
/*								*/
/*	This program returns the string:			*/
/*								*/
/*		TERM= . . .					*/
/*								*/
/****************************************************************/
/*								*/
/*	Login(1) sets a default for $TERM, so for logging-in	*/
/*	to any terminal place the following in ".profile":	*/
/*								*/
/*		eval `tset`					*/
/*								*/
/*	To change $TERM during a session:			*/
/*								*/
/*		eval `tset device_type`				*/
/*								*/
/****************************************************************/

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void Find_Termcap, (char *terminal));
_PROTOTYPE(void Error, (char *msg));


int main(argc, argv)
int argc;
char *argv[];
{
  char *name;
  FILE *f;
  char line[LINE_LENGTH];


  if (argc > 2) {
	fprintf(stderr, "Usage:  %s  [ device_type ]\n", argv[0]);
	exit(1);
  }
  if (argc == 2) {
	Find_Termcap(argv[1]);
	exit(0);
  }

  /* No terminal name supplied, so use the current device	 */

  if ((name = ttyname(0)) == NULL)
	Error("Can not determine the user's terminal");

  name += 5;			/* Chop off "/dev/" part	 */

  /* Look up the default terminal type in /etc/ttytype		 */

  if ((f = fopen("/etc/ttytype", "r")) == NULL)
	Error("Can not open /etc/ttytype");

  while (fgets(line, LINE_LENGTH, f) != NULL) {
	char *space = strchr(line, ' ');

	line[strlen(line) - 1] = '\0';	/* Remove '\n'		 */

	if (strcmp(space + 1, name) == 0) {
		*space = '\0';
		Find_Termcap(line);
		exit(0);
	}
  }

  Error("Can not find your terminal in /etc/ttytype");
  return(0);
}

void Find_Termcap(terminal)
char *terminal;
{
  char termcap[TC_BUFFER];

  if (tgetent(termcap, terminal) != 1)
	Error("No termcap for your terminal type");

  /* In real Unix the $TERMCAP would also be returned here  */
  printf("TERM=%s;\n", terminal);
}

void Error(msg)
char *msg;
{
  fprintf(stderr, "tset: %s\n", msg);
  exit(1);
}
