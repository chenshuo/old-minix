#include "stdio.h"

int puts(s)
char *s;
{
    char c;

    c = fputs(s,stdout);
    putchar('\n');
    return(c);
}
