/*  getlogin(3)
 *
 *  Author: Terrence W. Holm          Aug. 1988
 */

#include <stdio.h>
#include <pwd.h>

#ifndef  L_cuserid
#define  L_cuserid   9
#endif

extern struct passwd *getpwuid();


char *getlogin()
  {
  static  char   userid[ L_cuserid ];
  struct passwd *pw_entry;

  pw_entry = getpwuid( getuid() );

  if ( pw_entry == NULL )
    return( NULL );

  strcpy( userid, pw_entry->pw_name );

  return( userid );
  }
