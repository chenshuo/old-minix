/*  strtok(3)
 *
 *  Author: Terrence W. Holm          July 1988
 *
 *
 *  This function is used to divide up a string into tokens.
 *  Strtok(3) is called with <string> pointing to the string
 *  to be scanned and <char_set> pointing to a string which
 *  consists of the set of separator characters. Tokens are
 *  substrings bordered by separator characters. A pointer to
 *  the first token encountered is returned. If <string> is
 *  NULL then the scan is continued from the last token
 *  returned. Each token is terminated by a '\0'. If there are
 *  no tokens remaining in the string then NULL is returned.
 */

#define NULL (char *) 0


char *strtok( string, char_set )
  char *string;
  char *char_set;

  {
  static char *last_string = "";
  register char *chr;
  char *next_token;

  if ( string == NULL )
      string = last_string;

  if ( char_set == NULL )
      return( NULL );


  /*  First skip over any separator characters  */

  while ( *string != '\0' )
      {
      for ( chr = char_set;  *chr != '\0';  ++chr )
	  if ( *string == *chr )
	      break;

      if ( *chr == '\0' )
	  break;

      ++string;
      }


  /*  Check if we have reached the end of the string  */

  if ( *string == '\0' )
      return( NULL );


  /*  If not, then we have found the next token  */

  next_token = string;


  /*  Scan for the end of this token  */

  while ( *string != '\0' )
      {
      for ( chr = char_set;  *chr != '\0';  ++chr )
	  if ( *string == *chr )
	      {
	      *string = '\0';
	      last_string = string + 1;
	      return( next_token );
	      }

      ++string;
      }

  last_string = string;
  return( next_token );
  }
