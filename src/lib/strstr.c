/*  strstr(3)
 *
 *  Author: Terrence W. Holm          July 1988
 *
 *
 *  Finds the first occurrence of a substring, pointed to by
 *  <substr>, within a string pointed to by <string>.
 *  If the substring is found then a pointer to it within
 *  <string> is returned, otherwise NULL is returned.
 */

#define NULL (char *) 0


char *strstr( string, substr )
  char *string;
  char *substr;

  {
  register char head_string;
  register char head_substr;

  if ( string == NULL  ||  substr == NULL )
      return( NULL );

  head_substr = *substr++;

  while ( (head_string = *string++) != '\0' )
    if ( head_string == head_substr )
	{
	register char *tail_string = string;
	register char *tail_substr = substr;

	do  {
	    if ( *tail_substr == '\0' )
		return( string - 1 );
	    } while ( *tail_string++ == *tail_substr++ );
	}

  return( NULL );
  }
