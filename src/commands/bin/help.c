/* help - provide assistance about keywords	Author: Wolf N. Paul */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAXLINE    134
#define HELPDIR    "/usr/lib"      /* Added by John Plocher */
#define HELPFILE   "helpfile"      /* .. */
#define INDEX      ".idx"          /* .. */

char *helpfilename = "/usr/lib/helpfile";
char *helpidxname  = "/usr/lib/helpfile.idx";

main(argc, argv)
int argc;
char **argv;
{
   struct
   {
       char name[15];
       long offset;
   } entry;                /* helpindex entries for each command */

   struct stat sbuf1, sbuf2;   /* stat buffers for helpfile & helpindex */
   char *command, *line, Line[MAXLINE];
   register char   *cp;        /* John Plocher */
   extern char *getenv();      /* .. */
   int status;
   FILE *ifp, *hfp;        /* file pointers for helpfile and helpindex */
   
   if (argc > 2) {
	fprintf(stderr, "Usage: help subject\n");
	exit(1);
   }

   if ( argc == 1 )        /* If no arguments, ... */
       command = "help";   /* ... default to "help help" */
   else
       command = argv[1];  /* else look for command in argv[1] */

   stat(helpfilename, &sbuf1); /* get mtime for helpfile */
   status=access(helpidxname, 0);
   if ( status == 0 )  /* if helpindex exists ... */
   {
       stat(helpidxname, &sbuf2);  /* get mtime for helpindex */
   }
   if ( (status != 0) ||           /* if there is no helpindex ... */
       (sbuf1.st_mtime > sbuf2.st_mtime) )
                                   /* or if it is older than helpfile */
   {
       buildindex();       /* build a new helpindex */
   }

   system("clr");		/* clear the screen */

   if ( (ifp=fopen(helpidxname, "r")) == NULL )
   {
       fprintf(stderr, "Can't read %s\n", helpidxname);
       exit(-1);
   }
   
   while ( 1 )     /* look for index entry for "command" */
   {
       status=fread(&entry, sizeof(entry), 1, ifp);
       if ( status==0 ) /* quit at end of index file */
       {
           fprintf(stderr, "No help for %s\n", command);
           fclose(ifp);
           exit(1);        }
       if ( strcmp(entry.name, command) == 0 ) /* quit when we find it */
       {
           fclose(ifp);
           break;
       }
   }

   if ((hfp=fopen(helpfilename, "r")) == NULL )
   {
       fprintf(stderr, "Can't open %s\n", helpfilename);
       exit(-1);
   }

   fseek(hfp, entry.offset, 0);    /* go to the help entry */

   while ( 1 )         /* just copy lines to stdout */
   {
       line = fgets(Line, MAXLINE, hfp);
       if ( line == (char *) NULL || line[0] == '#' )
                       /* until another entry starts */
           break;
       fputs(line,stdout);
   }

   fclose(hfp);
}

buildindex()
{
   FILE *hfp, *ifp;
   struct {
       char name[15];
       long offset;
   } entry;
   char Line[MAXLINE];
   char *line;
   int i,j;


   unlink(helpidxname); /* remove old index file */
   if ( (hfp=fopen(helpfilename, "r")) == NULL )
   {
       fprintf(stderr,"buildindex: Can't read %s\n", helpfilename);
       exit(-1);
   }
   if ( (ifp=fopen(helpidxname, "w")) == NULL )
   {
       fprintf(stderr, "buildindex: Can't write %s\n", helpidxname);
       exit(-1);
   }

   while (1)   /* Read thru helpfile ... */
   {
       entry.offset=(long) 0;
       line = fgets(Line, MAXLINE, hfp);
       if ( line == (char *) NULL ) break;
       if ( line[0] == '#' )   /* and for each help entry ... */
       {
           line++;
           while ( isspace(line[0]) ) line++;
           i=j=0;
           while ( line[i] != '\0' )
           {
               if ( line[i] == '\n' ) break;
               while ( line[i] == ' ' || line[i] == ',' ) i++;
               while ( !isspace(line[i] ) &&
                       line[i] != ',') /* save its name ... */
               {
                   entry.name[j] = line[i];
                   i++; j++;
               }
               while ( j < 15 )
                   entry.name[j++] = '\0';
               j = 0;
               entry.offset=ftell(hfp);    /* and its offset ... */
               fwrite(&entry, sizeof(entry), 1, ifp);
                                       /* and write it to indexfile */
           }
       }
   }
   fclose(hfp);
   fclose(ifp);
}
