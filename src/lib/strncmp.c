/*
  strncmp.c - int strncmp( char *s1, char *s2, int n )

  Strcmp  compares  s1  to  s2, up to at most n characters
          (lexicographically with native character comparison).
  It returns
    positive  if  s1 > s2
    zero      if  s1 = s2
    negative  if  s1 < s2.
*/

int strncmp( s1, s2, n )
register char *s1;
register char *s2;
int n;
{
  if ( n <= 0 )
    return 0;
  while ( *s1++ == *s2++ )
    if ( s1[-1] == 0 || --n == 0 )
      return 0;
  return s1[-1] - s2[-1];
}
