/* fdopen - YAH (yet another hack)  hacked from fopen.c */

#include <stdio.h>

#define  PMODE    0644


FILE *fdopen(fd,mode)
int  fd;
char *mode;
{
	register int i;
	FILE *fp;
	char *malloc();
	int flags = 0;

	for (i = 0; _io_table[i] != 0 ; i++) 
		if ( i >= NFILES )
			return(NULL);

	switch(*mode){

	case 'w':
		flags |= WRITEMODE;
		break;

        case 'a':
                flags |= WRITEMODE;
                lseek(fd,0L,2);
                break;

	case 'r':
		flags |= READMODE;	
		break;

	default:
		return(NULL);
	}


	if (( fp = (FILE *) malloc (sizeof( FILE))) == NULL )
		return(NULL);


	fp->_count = 0;
	fp->_fd = fd;
	fp->_flags = flags;
	fp->_buf = malloc( BUFSIZ );
	if ( fp->_buf == NULL )
		fp->_flags |=  UNBUFF;
	else 
		fp->_flags |= IOMYBUF;

	fp->_ptr = fp->_buf;
	_io_table[i] = fp;
	return(fp);
}
