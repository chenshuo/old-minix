/*  lsearch(3)  and  lfind(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */

#define  NULL  (char *) 0


char *lsearch( key, base, count, width, keycmp )
  char *key;
  char *base;
  unsigned *count;
  unsigned  width;
  int  (*keycmp)();

  {
  char *entry;
  char *last = base + *count * width;

  for ( entry = base;  entry < last;  entry += width )
    if ( keycmp( key, entry ) == 0 )
	return( entry );

  bcopy( key, last, width );
  *count += 1;
  return( last );
  }



char *lfind( key, base, count, width, keycmp )
  char *key;
  char *base;
  unsigned *count;
  unsigned  width;
  int  (*keycmp)();

  {
  char *entry;
  char *last = base + *count * width;

  for ( entry = base;  entry < last;  entry += width )
    if ( keycmp( key, entry ) == 0 )
	return( entry );

  return( NULL );
  }
