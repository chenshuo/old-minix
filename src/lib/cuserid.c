/*  cuserid(3)
 *
 *  Author: Terrence W. Holm          Sept. 1987
 */

#include <stdio.h>
#include <pwd.h>

#ifndef  L_cuserid
#define  L_cuserid   9
#endif

extern struct passwd *getpwuid();


char *cuserid( user_name )
  char *user_name;

  {
  static  char   userid[ L_cuserid ];
  struct passwd *pw_entry;

  if ( user_name == NULL )
    user_name = userid;

  pw_entry = getpwuid( geteuid() );

  if ( pw_entry == NULL )
    {
    *user_name = '\0';
    return( NULL );
    }

  strcpy( user_name, pw_entry->pw_name );

  return( user_name );
  }
