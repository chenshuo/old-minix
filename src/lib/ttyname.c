/*  ttyname(3)
 */

#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>

#define  NULL 	(char *) 0
#define  DEV	"/dev/"

static char file_name[40];


char *ttyname( tty_file_desc )
  int tty_file_desc;

  {
  int dev_dir_desc;
  struct direct dir_entry;
  struct stat tty_stat;
  struct stat dev_stat;

  /*  Make sure the supplied file descriptor is for a terminal  */

  if ( fstat(tty_file_desc, &tty_stat) < 0 )
	return( NULL );

  if ( (tty_stat.st_mode & S_IFMT) != S_IFCHR )
	return( NULL );


  /*  Open /dev for reading  */

  if ( (dev_dir_desc = open(DEV,0)) < 0 )
	return( NULL );

  while( read(dev_dir_desc, (char *) &dir_entry, sizeof (struct direct)) > 0 )
	{
	/*  Find an entry in /dev with the same inode number  */

	if ( tty_stat.st_ino != dir_entry.d_ino )
		continue;

	/*  Put the name of the device in static storage  */

	strcpy( file_name, DEV );
	strncat( file_name, dir_entry.d_name, sizeof (dir_entry.d_name) );

	/*  Be absolutely certain the inodes match  */

  	if ( stat(file_name, &dev_stat) < 0 )
		continue;

  	if ( (dev_stat.st_mode & S_IFMT) != S_IFCHR )
		continue;

  	if ( tty_stat.st_ino  == dev_stat.st_ino  &&
	     tty_stat.st_dev  == dev_stat.st_dev  &&
	     tty_stat.st_rdev == dev_stat.st_rdev )
		{
		close( dev_dir_desc );
		return( file_name );
		}
	}

  close( dev_dir_desc );
  return( NULL );
  }
