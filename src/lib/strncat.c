/*
  strncat.c - char *strncat( char *s1, char *s2, int n )

  Strncat  appends up to  n  characters  from  s2  to the end of  s1.
  It returns s1.
*/

char *strncat( s1, s2, n )
char *s1;
register char *s2;
int n;
{
  register char *rs1;

  rs1 = s1;
  if ( n > 0 )
  {
    while ( *rs1++ != 0 )
      ;
    --rs1;
    while ( (*rs1++ = *s2++) != 0 )
      if ( --n == 0 )
      {
        *rs1 = 0;
        break;
      }
  }
  return s1;
}
