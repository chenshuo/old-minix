/*  getenv(3)
 *
 *  Author: Terrence W. Holm          Aug. 1988
 */

#define  NULL   (char *) 0

extern char **environ;


char *getenv( name )
  char *name;
  {
  char **v;
  register char *n;
  register char *p;

  if ( environ == (char **) NULL  ||  name == NULL )
    return(NULL);

  for ( v = environ;  *v != NULL;  ++v )
    {
    n = name;
    p = *v;

    while ( *n == *p  &&  *n != '\0' )
	++n, ++p;

    if ( *n == '\0'  &&  *p == '=' )
	return( p + 1 );
    }

  return(NULL);
  }
