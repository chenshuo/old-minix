/*  bzero(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */


bzero( vector, count )
  char *vector;
  int   count;

  {
  int word_count = count / sizeof(int);
  int byte_count = count & ( sizeof(int) - 1 );

  while( --word_count >= 0 )
    *((int *) vector)++ = 0;

  while( --byte_count >= 0 )
    *vector++ = 0;
  }
