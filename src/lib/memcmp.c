/*  memcmp(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */


int memcmp( vector1, vector2, count )
  char *vector1;
  char *vector2;
  int   count;

  {
  register int cmp;

  if ( vector1 == vector2 )
    return( 0 );

  while( --count >= 0 )
    if ( cmp = *vector1++ - *vector2++ )
	return( cmp );

  return( 0 );
  }
