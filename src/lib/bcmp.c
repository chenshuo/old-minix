/*  bcmp(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */


int bcmp( vector1, vector2, count )
  char *vector1;
  char *vector2;
  int   count;

  {
  int word_count = count / sizeof(int);
  int byte_count = count & ( sizeof(int) - 1 );

  if ( vector1 == vector2 )
    return( 0 );

  while( --word_count >= 0 )
    if (  *((int *) vector1)++ != *((int *) vector2)++ )
	return( 1 );

  while( --byte_count >= 0 )
    if (  *vector1++ != *vector2++ )
	return( 1 );

  return( 0 );
  }
