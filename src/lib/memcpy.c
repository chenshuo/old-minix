/*  memcpy(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */


char *memcpy( to, from, count )
  char *to;
  char *from;
  int   count;

  {
  int word_count = count / sizeof(int);
  int byte_count = count & ( sizeof(int) - 1 );
  char *temp_to  = to;
  
  if ( to > from  &&  to < from + count )
    {
    /*  Must copy backwards  */
    from += count;
    to   += count;

    while( --byte_count >= 0 )
      *--to = *--from;

    while( --word_count >= 0 )
      *--((int *) to) = *--((int *) from);
    }
  else
    {
    while( --word_count >= 0 )
      *((int *) to)++ = *((int *) from)++;

    while( --byte_count >= 0 )
      *to++ = *from++;
    }

  return( temp_to );
  }
