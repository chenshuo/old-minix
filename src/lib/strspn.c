/*  strspn(3)
 *
 *  Author:  Terrence W. Holm          July 1988
 *
 *
 *  This function determines the length of a span from the
 *  beginning of <string> which contains only characters
 *  specified in <char_set>. The length of the span is
 *  returned.
 */

#define NULL (char *) 0


int strspn( string, char_set )
  char *string;
  char *char_set;

  {
  register char *str;
  register char *chr;

  if ( string == NULL  ||  char_set == NULL )
      return( 0 );

  for ( str = string;  *str != '\0';  ++str )
      {
      for ( chr = char_set;  *chr != '\0';  ++chr )
	  if ( *str == *chr )
	      break;

      if ( *chr == '\0' )
	  return( str - string );
      }

  return( str - string );
  }
