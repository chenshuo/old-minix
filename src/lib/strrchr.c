/*  strrchr(3) 
 *
 *  Derived from MINIX rindex(3)
 */


#define  NULL  (char *) 0


char *strrchr( string, chr )
  register char *string;
  register char  chr;

  {
  char *index = NULL;

  do {
     if ( *string == chr )
	index = string;
     } while ( *string++ != '\0' );

  return( index );
  }
