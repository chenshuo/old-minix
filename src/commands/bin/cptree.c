#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>

#define DIRBUF  64          /* The number of file entries read in one blow */

#define MAX_PATH_L  1000    /* The maximum pathname length */

#define HASHLIST    64      /* The size of the hash list used for link info */

#define MKDIR   "/bin/mkdir"

struct linktab {
	struct linktab *l_next ;        /* chain */
	short   l_nlink ;
	dev_t   l_dev ;
	ino_t   l_ino ;
} ;

extern char *malloc() ;

char    *sysmes() ;

char    *prog ;
int     root ;          /* true if super-user, false otherwise */

struct  linktab *linkhead[HASHLIST]  ;

struct  direct  backfile ;      /* used to read directories when */
				/* memory is exhausted */
unsigned        dirlev=0 ;      /* Directory distance from start */

char    in_path[MAX_PATH_L] ;
char    out_path[MAX_PATH_L] ;

dev_t   src_dev ;
ino_t   src_ino ;
dev_t   dst_dev ;
ino_t   dst_ino ;

int     p_flag = 0 ;    /* print all actions */
int     d_flag = 0 ;    /* do not cross device boundaries on in tree */
int     dd_flag= 0 ;    /* do not cross device boundaries on out tree */
int     ll_flag= 0 ;    /* try to link the files to the original */
int     l_flag = 0 ;    /* don't try to link files */
int     f_flag = 0 ;    /* don't complain about existing destinations */
int     t_flag = 0 ;    /* don't set accessed/update times */
int     r_flag = 0 ;    /* retain all existing destinations */

main(argc,argv) char **argv ; {
	char *in, *out ;
	int  cnt,i ;

	in=out= (char *)0 ;
	root= geteuid()==0 ;
	prog=argv[0] ;
	for ( cnt=1 ; cnt<argc ; cnt++ ) {
		if ( argv[cnt][0]=='-' ) {
			for ( i=1 ; argv[cnt][i] ; i++ )
			switch ( argv[cnt][i]) {
			case 'p' : p_flag++ ; break ;
			case 'd' : d_flag++ ; break ;
			case 'D' : dd_flag++ ; break ;
			case 'f' : f_flag++ ; break ;
			case 't' : t_flag++ ; break ;
			case 'r' : r_flag++ ; break ;
			case 'l' : l_flag++ ; break ;
			case 'L' : ll_flag++ ; break ;
			default  : fatal("Illegal flag %s",argv[cnt]) ;
			}
		} else {
			if ( !in ) { in=argv[cnt] ; }
			else if ( !out ) { out=argv[cnt] ; }
			else { fatal("Too many arguments") ; }
		}
	}
	if ( !out ) {
		fprintf(stderr,"Usage: %s [-pDdLlftr] from to\n",prog) ;
		exit(1) ;
	}
	if ( f_flag && r_flag ) fatal("-f and -r flags can not be combined") ;
	if ( l_flag && ll_flag ) fatal("-l and -L flags can't be combined") ;
	strncpy(in_path,in,MAX_PATH_L) ;
	strncpy(out_path,out,MAX_PATH_L) ;
	if ( chkdir(in,1) && chkdir(out,0) )
		return cptree(strlen(in_path),strlen(out_path)) ;
	return 1 ;
}

#define NOFILE  -1

int cptree(sz_in,sz_out) int sz_in, sz_out ; {
	off_t diroffset ;
	struct direct *files ;
	unsigned n_files ;
	int retv ;
	int fildes ;
	int c_count, f_count ;
	int i ;

	diroffset=0 ;
	fildes= NOFILE ;
	n_files=0 ;
	for (;;) {
		if ( fildes==NOFILE ) {
			fildes=open(in_path,0) ;
			if ( fildes== -1 ) {
				mess("cannot open %s - %s",
				      in_path,sysmes()) ;
				retv= -1 ; break ;
			}
			if ( diroffset!=0 ) {
				if ( lseek(fildes,(long) diroffset,0)== -1 ) {
					mess("cannot seek %s - %s",
					      in_path,sysmes()) ;
					retv= -1 ; break ;
				}
			}
		}
		if ( n_files==0 ) {
			/* Allocate space to read in filename */
			if ( dirlev++>=4 ) {
				/* I do not want to allocate too much room
				   too the directory buffers, so when
				   it gets too deep, I always use
				   the same entry, which can be overwritten !
				*/
				n_files=1 ;
				files= &backfile ;
			} else {
				files= (struct direct *)
				     malloc(DIRBUF*sizeof (struct direct));
				if ( (char *)files== -1 ) {
					n_files=1 ;
					files= &backfile ;
				} else {
					n_files=DIRBUF ;
				}
			}
		}
		c_count= read(fildes,files,n_files*sizeof (struct direct)) ;
		if ( c_count<0 ) {
			mess("read failed %s - %s",in_path,sysmes()) ;
		}
		if ( c_count<=0 ) {
			if ( close(fildes)==-1 ) {
				mess("close dir %s %d",in_path,fildes) ;
			}
			retv=0 ; break ;
		}
		f_count= c_count/sizeof (struct direct) ;
		if ( c_count%sizeof (struct direct) ) {
			mess ("awkward directory size %s",in_path) ;
			if ( close(fildes)==-1 ) {
				mess("close dir %s %d",in_path,fildes) ;
			}
			retv= -1 ; break ;
		}
		diroffset+= c_count ;
		for ( i=0 ; i<f_count ; i++ ) {
			/* Finally at each entry */
			entry(sz_in,sz_out,&files[i],&fildes) ;
		}
	}
	if ( n_files ) {
		dirlev-- ;
		if ( files!= &backfile ) free(files) ;
	}
	return retv ;
}

entry(sz_in,sz_out,file,fd)
	int sz_in, sz_out ;
	struct direct *file ;
	int *fd ; {

	static char name[DIRSIZ+1] ;
	int sz_name, sz_in_new, sz_out_new ;
	register int i ;

	if ( file->d_ino==0 ) return ;
	for ( i=0 ; i<DIRSIZ ; i++ ) name[i]=file->d_name[i] ;
	name[DIRSIZ]=0 ;
	if ( name[0]=='.' ) { /* ignore "." and ".." */
		if ( name[1]==0 ) return ;
		if ( name[1]=='.' && name[2]==0 ) return ;
	}

	sz_name=strlen(name) ;
	sz_in_new= sz_in ; sz_out_new= sz_out ;
	if ( sz_in!=0 && in_path[sz_in-1]=='/' ) sz_in_new-- ;
	if ( sz_out!=0 && out_path[sz_out-1]=='/' ) sz_out_new-- ;
	if ( sz_in_new+sz_name+2>=MAX_PATH_L )
		fatal("Filename too long: %s",in_path) ;
	if ( sz_out_new+sz_name+2>=MAX_PATH_L )
		fatal("Filename too long: %s",out_path) ;

	if ( sz_in!=0 ) in_path[sz_in_new++]='/' ;
	if ( sz_out!=0 ) out_path[sz_out_new++]='/' ;
	for ( i=0 ; i<=sz_name ; i++ ) {
		in_path[sz_in_new++]= name[i] ;
		out_path[sz_out_new++]= name[i] ;
	}
	/* WARNING: After this point the contents of name and *file
	   cannot be trsted */

	act(sz_in_new-1,sz_out_new-1,fd) ;
	in_path[sz_in]=0 ; out_path[sz_out]=0 ;
}

act(sz_in,sz_out,fd) int sz_in, sz_out, *fd ; {
	struct stat instat, outstat ;
	int ostatok ;
	unsigned short f_type ;
	int touched ;
	int inladm ;

	if ( stat(in_path,&instat)== -1 ) {
		mess("can't stat %s - %s",in_path,sysmes()) ;
		return ;
	}
	ostatok= stat(out_path,&outstat)==0 ;

	f_type= instat.st_mode&S_IFMT ;
	if ( f_type==S_IFDIR ) {
		if ( instat.st_ino==dst_ino && instat.st_dev==dst_dev ) {
			mess("recursive copy attempted at %s",in_path) ;
			return ;
		}
		if ( ostatok &&
		     outstat.st_ino==src_ino && outstat.st_dev==src_dev ) {
			mess("recursive copy attempted at %s",in_path) ;
			return ;
		}
		if ( d_flag && instat.st_dev!=src_dev ) {
			note("Stopped at %s",in_path) ;
			return ;
		}
		if ( dd_flag && ostatok && outstat.st_dev!=dst_dev ) {
			note("Stopped at %s",out_path) ;
			return ;
		}
		if ( ostatok ) {
			if ( (outstat.st_mode&S_IFMT)!=S_IFDIR) {
				if ( !remove() ) return ;
			}
		}
		if ( *fd!=NOFILE ) {
			if ( close(*fd)==-1 ) {
				mess("can't close parent of %s %d",
				     in_path,*fd) ;
			}
			*fd= NOFILE ;
		}
		/* close the parent dir */
		touched=d_copy(sz_in,sz_out,&instat,ostatok,&outstat) ;
	} else {
		if ( ostatok ) {
			if ( (outstat.st_mode&S_IFMT)!=S_IFDIR) {
				if ( !remove() ) return ;
			} else {
				mess("directory %s - not removed",out_path);
				return ;
			}
		} /* The destination file should not exist now */
		if ( ll_flag ) {
			ll_link() ;
			return ;
		}
		inladm=1 ;
		if ( instat.st_nlink>1 && !l_flag &&
		      (inladm=can_link(&instat))==0 ) return ;
		if ( f_type==S_IFREG ) {
			touched=f_copy(&instat) ;
		} else {
			/* special node */
			touched=s_copy(&instat) ;
		}
		if ( touched && inladm==2 )
			set_link(sz_out,&instat) ;
	}
	if ( !touched ) return ;
	/* Set mode & owner */
	if ( root ) {
		if ( chown(out_path,instat.st_uid,instat.st_gid)!=0 ) {
			mess("can't change owner %s - %s",out_path,sysmes());
		}
	}
	if ( chmod(out_path,instat.st_mode)!=0 ) {
		mess("can't change mode %s - %s",out_path,sysmes()) ;
	}
	if ( t_flag==0 ) {
		if ( utime(out_path,&instat.st_atime)== -1 ) {
			mess("can't set access/update times %s -%s",
			     out_path,sysmes()) ;
		}
	}
}

int f_copy(nstat) struct stat *nstat ; {
	int kar ;
	union {
		char read_b[BUFSIZ] ;
		int  chk_b[BUFSIZ/sizeof (int)];
	} buffer ;
	int inp, outp, incount, outcount ;
	long trail_zero ; /* number of consecutive zero seen so far */

	inp=open(in_path,0) ;
	if ( inp<0 ) {
		mess("can't open %s - %s",in_path,sysmes()) ;
		return 0 ;
	}
	outp=creat(out_path,0) ;
	if ( outp<0 ) {
		mess("can't create %s - %s",out_path,sysmes()) ;
		close(inp) ;
		return 0 ;
	}
	note("Copying %s to %s",in_path,out_path) ;
	trail_zero=0 ;
	for (;;) {
		incount=read(inp,buffer.read_b,BUFSIZ) ;
		if ( incount<=0 ) {
			if ( incount<0 ) {
				mess("read error on %s - %s",
				     out_path,sysmes()) ;
			}
			break ;
		}
		if ( buffer.chk_b[0]==0 && chkzero(buffer.chk_b,incount) ) {
			trail_zero += incount ;
		} else {
			if ( trail_zero ) {
				if ( lseek(outp,trail_zero,1)== -1 ) {
					mess("seek error on %s - %s",
						out_path,sysmes()) ;
					break ;
				}
				trail_zero=0 ;
			}
			outcount=write(outp,buffer.read_b,incount) ;
			if ( outcount!=incount ) {
				mess("write error on %s - %s",
					out_path,sysmes()) ;
				break ;
			}
		}
	}
	if ( trail_zero ) {
		if ( lseek(outp,trail_zero-1,1)== -1 ) {
			mess("seek error on %s - %s",
				out_path,sysmes()) ;
		} else {
			/* Note: buffer.read_b[0] must be 0 */
			if ( write(outp,buffer.read_b,1)!=1 ) {
				mess("write error on %s - %s",
					out_path,sysmes()) ;
			}
		}
	}
	if ( close(inp)!=0 ) {
		mess("close %s failed - %s",in_path,sysmes()) ;
	}
	if ( close(outp)!=0 ) {
		mess("close %s failed - %s",out_path,sysmes()) ;
	}
	return 1 ;
}

int s_copy(nstat) struct stat *nstat ; {
	if ( mknod(out_path,nstat->st_mode,nstat->st_rdev)==0 ) {
		note("Created node %s",out_path) ;
		return 1 ;
	}
	mess("can't make node %s - %s",out_path,sysmes()) ;
	return 0 ;
}

int d_copy(sz_in,sz_out,instat,ostatok,outstat)
	struct  stat *instat, *outstat ;
	int     sz_in, sz_out, ostatok ;
{
	int     pid, rpid, status ;

	if ( !ostatok ) {
		note("Making directory %s",out_path) ;
		while ( (pid=fork())== -1 );
		if ( pid ) {
			/* parent */
			do {
				rpid=wait(&status) ;
				if ( rpid==-1 ) {
					mess("my children disappear") ;
					break ;
				}
			} while ( rpid!=pid ) ;
			if ( (status>>8)!=0 ) {
				mess("mkdir failed on %s",out_path) ;
			}
		} else {
			/* the child */
			dup2(1,5) ; dup2(2,6) ; close(1) ; close(2) ;
			execl(MKDIR,"mkdir",out_path,0) ;
			dup(5) ; dup(6) ;
			mess("can't execute mkdir") ;
			exit(8) ;
		}
		if ( !chkdir(out_path,2) ) return 0 ;
		chmod(out_path,0700) ; /* For security's sake */
	} else {
		chmod(out_path,0700) ; /* For security's sake */
		if ( access(out_path,3)==0 ) { /* search, write access */
			note("Directory %s already exists",out_path) ;
		} else {
			mess("can't create files in directory %s - %s",
				out_path,sysmes()) ;
			/* The chmod needs not be undone, because it didn't
			   have the desired effect to start with */
			return 0 ;
		}
	}
	cptree(sz_in,sz_out) ;
	return 1 ;
}

int remove() {
	if ( r_flag ) {
		note("Retained %s",out_path) ;
		return 0 ;
	}
	if ( !f_flag ) {
		fatal("needs -f flag") ;
	}
	if ( unlink(out_path)==0 ) {
		note("Removed %s",out_path) ;
		return 1 ;
	} else {
		mess("can't remove %s - %s",out_path,sysmes()) ;
	}
	return 0 ;
}

set_link(sz_out,instat) struct stat *instat ; {
	struct linktab *linkent ;
	static int nomem = 0 ;
	extern char *malloc() ;

	linkent = (struct linktab *)malloc(sizeof *linkent + sz_out + 1 ) ;
	if ( (char *)linkent == -1 ) {
		if ( nomem==0 ) {
			mess("not enough memory for the link information") ;
			nomem= 1 ;
		}
		return ;
	}
	linkent->l_ino= instat->st_ino ;
	linkent->l_dev= instat->st_dev ;
	linkent->l_nlink= instat->st_nlink-1 ;
	strcpy((char *)(linkent+1),out_path) ;
	linkent->l_next= linkhead[instat->st_ino%HASHLIST] ;
	linkhead[instat->st_ino%HASHLIST]= linkent ;
}

int can_link(instat) struct stat *instat ; {
	register struct linktab *ln, *lp ;
	int result ;

	lp= (struct linktab *)0 ;
	for ( ln=linkhead[instat->st_ino%HASHLIST] ; ln ; ln= ln->l_next ) {
		if ( instat->st_ino == ln->l_ino &&
		     instat->st_dev == ln->l_dev ) {
			/* Found an already copied file */
			result= link((char *)(ln+1),out_path)==0 ;
			if ( result ) {
				note("Linked %s to %s",
				     out_path,(char *)(ln+1)) ;
			} else {
				mess("can't link %s to %s - %s",
				     out_path,(char *)(ln+1),sysmes()) ;
			}
			if ( --ln->l_nlink == 0 ) {
				if ( lp ) lp->l_next= ln->l_next ;
				else linkhead[instat->st_ino%HASHLIST]=
					ln->l_next ;
				free(ln) ;
			}
			return result ? 0 : 1 ;
		}
		lp= ln ;
	}
	return 2 ;
}

int ll_link() {
	if ( link(in_path,out_path)==0 ) {
		note("Linking %s to %s",in_path,out_path) ;
		return 1 ;
	}
	mess("can't link %s to %s",in_path,out_path) ;
	return 0 ;
}

int chkdir(name,flag) char *name ; {
	struct stat nstat ;

	if ( stat(name,&nstat)== -1 ) {
		mess("cannot stat %s - %s",name,sysmes()) ;
		return 0 ;
	}
	if ( (nstat.st_mode&S_IFMT)!=S_IFDIR ) {
		mess("%s - not a directory",name) ;
		return 0 ;
	}
	if ( flag==1 ) {
		src_dev= nstat.st_dev ;
		src_ino= nstat.st_ino ;
	} else {
		if ( flag!=0 ) return 1 ;
		dst_dev= nstat.st_dev ;
		dst_ino= nstat.st_ino ;
		if ( src_dev==dst_dev && src_ino==dst_ino ) {
			mess("copying directory to itself") ;
			return 0 ;
		}
	}
	return 1 ;
}
int chkzero(buffer,size) int *buffer ; int size ; {
	register int *ptr, *end ;
	register char *c_end ;
	int r_size ;

	c_end=((char *)buffer)+size ;
	r_size=size%sizeof (int) ;
	while ( r_size-- ) if ( *--c_end ) return 0 ;
	end= (int *)c_end ;
	for ( ptr=buffer ; ptr<end ; ptr++ ) if ( *ptr ) return 0 ;
	return 1 ;
}

char *sysmes() {
	extern int errno ;
	extern int sys_nerr ;
	extern char *sys_errlist[];

	if ( errno==0 ) return "No mess" ;
	if ( errno>=sys_nerr ) return "Unknown mess" ;
	return sys_errlist[errno] ;
}

mess(fs,p1,p2,p3,p4,p5,p6,p7) char *fs ; {
	if ( p_flag ) {
		printf(fs,p1,p2,p3,p4,p5,p6,p7) ;
		printf("\n") ;
		return ;
	}
	fprintf(stderr,"%s: ",prog) ;
	fprintf(stderr,fs,p1,p2,p3,p4,p5,p6,p7) ;
	fprintf(stderr,"\n") ;
}

note(fs,p1,p2,p3,p4,p5,p6,p7) char *fs ; {
	if ( !p_flag ) return ;
	mess(fs,p1,p2,p3,p4,p5,p6,p7) ;
}

fatal(fs,p1,p2,p3,p4,p5,p6,p7) char *fs ; {
	mess(fs,p1,p2,p3,p4,p5,p6,p7) ;
	exit(8) ;
}
