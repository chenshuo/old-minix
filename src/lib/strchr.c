/*  strchr(3) 
 *
 *  Derived from MINIX index(3)
 */


#define  NULL  (char *) 0


char *strchr( string, chr )
  register char *string;
  register char  chr;

  {
  do {
     if ( *string == chr )
	return( string );
     } while ( *string++ != '\0' );

  return( NULL );
  }
