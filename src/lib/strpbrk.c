/*  strpbrk(3)
 *
 *  Author:  Terrence W. Holm          July 1988
 *
 *
 *  Strpbrk(3) scans <string> for the first occurrence of a
 *  character from the string <char_set>. If a character from
 *  the <char_set> was found then a pointer to it within
 *  <string> is returned, otherwise NULL is returned.
 */

#define NULL (char *) 0


char *strpbrk( string, char_set )
  char *string;
  char *char_set;

  {
  register char  c;
  register char *p;

  if ( string == NULL  ||  char_set == NULL )
      return( NULL );

  while ( (c = *string++) != '\0' )
      for ( p = char_set;  *p != '\0';  ++p )
	  if ( c == *p )
	    return( string - 1 );

  return( NULL );
  }
