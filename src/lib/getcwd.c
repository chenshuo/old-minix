/*  getcwd(3)
 *
 *  Author: Terrence W. Holm          Aug. 1988
 *
 *  Directly derived from Adri Koppes' pwd(1).
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <errno.h>

#define  NULL         (char *) 0
#define  O_RDONLY     0
#define  DIRECT_SIZE  (sizeof (struct direct))
#define  PATH_MAX     127

extern char *rindex();

extern int errno;


char *getcwd( buffer, size )
  char *buffer;
  int   size;

  {
  static char path[ PATH_MAX + 1 ];
  struct stat current;

  if ( buffer == NULL  ||  size == 0 )
    {
    errno = EINVAL;
    return( NULL );
    }

  path[0] = '\0';

  /*  Get the inode for the current directory  */
   
  if ( stat( ".", &current ) == -1 )
    return( NULL );

  if ( (current.st_mode & S_IFMT) != S_IFDIR )
    return( NULL );


  /*  Run backwards up the directory tree, grabbing 	*/
  /*  directory names on the way.			*/

  while (1)
    {
    struct stat parent;
    struct direct d;
    int same_device = 0;
    int found = 0;
    int fd;

    /*  Get the inode for the parent directory  */

    if ( chdir( ".." ) == -1 )
	return( NULL );

    if ( stat( ".", &parent ) == -1 )
	return( NULL );

    if ( (parent.st_mode & S_IFMT) != S_IFDIR )
	return( NULL );

    if ( current.st_dev == parent.st_dev )
	same_device = 1;


    /*  At the root, "." is the same as ".."  */

    if ( same_device  &&  current.st_ino == parent.st_ino )
	break;


    /*  Search the parent directory for the current entry  */

    if ( (fd = open( ".", O_RDONLY )) == -1 )
	return( NULL );

    while ( ! found  &&  read(fd, &d, DIRECT_SIZE) == DIRECT_SIZE )
	{
	if ( same_device )
	    {
	    if ( current.st_ino == d.d_ino )
		found = 1;
	    }
	else
	    {
	    static char temp_name[ DIRSIZ + 1 ];
	    static struct stat dir_entry;

	    temp_name[0] = '\0';
	    strncat( temp_name, d.d_name, DIRSIZ );

	    if ( stat( temp_name, &dir_entry ) == -1 )
		{
		close( fd );
		return( NULL );
		}

	    if ( current.st_dev == dir_entry.st_dev  &&
	         current.st_ino == dir_entry.st_ino )
		found = 1;
	    }
	}

    close( fd );

    if ( ! found )
    	return( NULL );

    if ( strlen(path) + DIRSIZ + 1 > PATH_MAX )
	{
	errno = ERANGE;
	return( NULL );
	}

    strcat( path, "/" );
    strncat( path, d.d_name, DIRSIZ );
 
    current.st_dev = parent.st_dev;
    current.st_ino = parent.st_ino;
    }


  /*  Copy the reversed path name into <buffer>  */

  if ( strlen(path) + 1 > size )
    {
    errno = ERANGE;
    return( NULL );
    }

  if ( strlen(path) == 0 )
    {
    strcpy( buffer, "/" );
    return( buffer );
    }

  *buffer = '\0';

  {
  char *r;

  while ( (r = rindex( path, '/' )) != NULL )
    {
    strcat( buffer, r );
    *r = '\0';
    }
  }

  return( chdir( buffer ) ? NULL : buffer );
  }
