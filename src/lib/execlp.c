/* execlp(3) and execvp(3)
 *
 * Author: Terrence W. Holm      July 1988
 */

/*
 *  Execlp(3) and execvp(3) are like execl(3) and execv(3),
 *  except that they use the environment variable $PATH as
 *  a search list of possible locations for the executable
 *  file, if <file> does not start with a '/'.
 *
 *  The path search list is a list of directory names separated
 *  by ':'s. If a colon appears at the beginning or end of the
 *  list, or two appear together, then an empty prefix is
 *  tried. If $PATH is not in the environment, it defaults to
 *  ":/bin:/usr/bin".
 *
 *  For example, if <file> is "sh", and the $PATH is
 *  ":/bin:/usr/local:/usr/bin", then the attempts will be:
 *  "sh", "/bin/sh", "/usr/local/sh" and "/usr/bin/sh".
 *
 *  If the <file> is not an executable file in one of the
 *  directories, then -1 is returned.
 */

#include <errno.h>
/* #include <string.h> */
/* #include <unistd.h> */

#ifndef X_OK
#define X_OK 1
#endif

#define  NULL  (char *) 0

extern char *index();
extern char *getenv();

extern char **environ;
extern int    errno;


execlp( file, arg0 )
  char *file;
  char *arg0;

  {
  return( execvp( file, &arg0 ) );
  }


execvp( file, argv )
  char *file;
  char *argv[];

  {
  char path_name[100];
  char *next;
  char *path = getenv( "PATH" );

  if ( path == NULL )
    path = ":/bin:/usr/bin";

  if ( file[0] == '/' )
    path = "";

  do  {
      next = index( path, ':' );

      if ( next == NULL )
	  strcpy( path_name, path );
      else
	  {
	  *path_name = '\0';
	  strncat( path_name, path, next - path );
	  path = next + 1;
	  }

      if ( *path_name != '\0' )
          strcat( path_name, "/" );

      strcat( path_name, file );

      if ( access( path_name, X_OK ) == 0 )
	  execve( path_name, argv, environ );
      } while ( next != NULL );

  errno = ENOENT;
  return( -1 );
  }
