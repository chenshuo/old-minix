/*
  strncpy.c - char *strncpy( char *s1, char *s2, int n )

  Strncpy  writes exactly  n  (or 0 if n < 0)  characters to  s1.
  It copies up to  n  characters from  s2, and null-pads the rest.
  The result is null terminated iff  strlen( s2 ) < n.
  It returns the target string.
*/

char *strncpy( s1, s2, n )
char *s1;
register char *s2;
int n;
{
  register char *rs1;

  rs1 = s1;
  if ( n > 0 )
  {
    while ( (*rs1++ = *s2++) != 0 && --n != 0 )  
      ;
	if ( n != 0 )
      while ( --n != 0 )
        *rs1++ = 0;
  }
  return s1;
}
