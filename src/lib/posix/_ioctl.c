#include <lib.h>
#define ioctl	_ioctl
#include <minix/com.h>
#include <sgtty.h>
#include <sys/ioctl.h>

PUBLIC int ioctl(fd, request, data)
int fd;
int request;
void *data;
{
  int n;
  long erase, kill, intr, quit, xon, xoff, eof, brk, speed;
  struct sgttyb *argp;
  struct tchars *argt;
  message m;

  m.TTY_LINE = fd;
  m.TTY_REQUEST = request;

  switch(request) {
     case TIOCSETP:
	argp = (struct sgttyb *) data;
	erase = argp->sg_erase & BYTE;
	kill = argp->sg_kill & BYTE;
	speed = ((argp->sg_ospeed & BYTE) << 8) | (argp->sg_ispeed & BYTE);
	m.TTY_SPEK = (speed << 16) | (erase << 8) | kill;
	m.TTY_FLAGS = argp->sg_flags;
	return(_syscall(FS, IOCTL, &m));

     case TIOCSETC:
	argt = (struct tchars *) data;
  	intr = argt->t_intrc & BYTE;
  	quit = argt->t_quitc & BYTE;
  	xon  = argt->t_startc & BYTE;
  	xoff = argt->t_stopc & BYTE;
  	eof  = argt->t_eofc & BYTE;
  	brk  = argt->t_brkc & BYTE;		/* not used at the moment */
  	m.TTY_SPEK = (intr<<24) | (quit<<16) | (xon<<8) | (xoff<<0);
  	m.TTY_FLAGS = (eof<<8) | (brk<<0);
	return(_syscall(FS, IOCTL, &m));

     case TIOCGETP:
	n = _syscall(FS, IOCTL, &m);
	argp = (struct sgttyb *) data;
	argp->sg_erase = (m.TTY_SPEK >> 8) & BYTE;
	argp->sg_kill  = (m.TTY_SPEK >> 0) & BYTE;
  	argp->sg_flags = m.TTY_FLAGS & 0xFFFFL;
	speed = (m.TTY_SPEK >> 16) & 0xFFFFL;
	argp->sg_ispeed = speed & BYTE;
	argp->sg_ospeed = (speed >> 8) & BYTE;
  	return(n);

     case TIOCGETC:
  	n = _syscall(FS, IOCTL, &m);
	argt = (struct tchars *) data;
  	argt->t_intrc  = (m.TTY_SPEK >> 24) & BYTE;
  	argt->t_quitc  = (m.TTY_SPEK >> 16) & BYTE;
  	argt->t_startc = (m.TTY_SPEK >>  8) & BYTE;
  	argt->t_stopc  = (m.TTY_SPEK >>  0) & BYTE;
  	argt->t_eofc   = (m.TTY_FLAGS >> 8) & BYTE;
  	argt->t_brkc   = (m.TTY_FLAGS >> 0) & BYTE;
  	return(n);

     default:
	m.ADDRESS = (char *) data;
	return(_syscall(FS, IOCTL, &m));
  }
}
