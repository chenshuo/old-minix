/*
 * TNET		A server program for MINIX which implements the TCP/IP
 *		suite of networking protocols.  It is based on the
 *		TCP/IP code written by Phil Karn et al, as found in
 *		his NET package for Packet Radio communications.
 *
 *		This module handles telnet option processing.
 *
 * Author:	Michael Temari, <temari@temari.ae.ge.com>  01/13/93
 *
 */
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "telnetd.h"
#include "telnet.h"

_PROTOTYPE(static int DoTelOpt, (int fdout, int c));
_PROTOTYPE(static void dowill, (int c));
_PROTOTYPE(static void dowont, (int c));
_PROTOTYPE(static void dodo, (int c));
_PROTOTYPE(static void dodont, (int c));
_PROTOTYPE(static void respond, (int ack, int option));

#define	LASTTELOPT	TELOPT_SGA

static int TelROpts[LASTTELOPT+1];
static int TelLOpts[LASTTELOPT+1];

static int TelOpts;

static int ThisOpt;

static int telfdout;

void tel_init()
{
int i;

   for(i = 0; i <= LASTTELOPT; i++) {
	TelROpts[i] = 0;
	TelLOpts[i] = 0;
   }
   TelOpts = 0;
}

void telopt(fdout, what, option)
int fdout;
int what;
int option;
{
char buf[3];
int len;

   buf[0] = IAC;
   buf[1] = what;
   buf[2] = option;
   len = 0;

   switch(what) {
	case DO:
		if(option <= LASTTELOPT) {
			TelROpts[option] = 1;
			len = 3;
		}
		break;
	case DONT:
		if(option <= LASTTELOPT) {
			TelROpts[option] = 1;
			len = 3;
		}
		break;
	case WILL:
		if(option <= LASTTELOPT) {
			TelLOpts[option] = 1;
			len = 3;
		}
		break;
	case WONT:
		if(option <= LASTTELOPT) {
			TelLOpts[option] = 1;
			len = 3;
		}
		break;
   }
   if(len > 0)
	(void) write(fdout, buf, len);
}

int tel_in(fdout, telout, buffer, len)
int fdout;
int telout;
char *buffer;
int len;
{
unsigned char *p, *p2;
int size, got_iac;

   telfdout = telout;
   p = (unsigned char *)buffer;

   while(len > 0) {
	while(len > 0 && TelOpts) {
		DoTelOpt(fdout, (int)*p);
		p++;
		len--;
	}
	if(len == 0) break;
	size = 0; p2 = p; got_iac = 0;
	while(len--) {
		if(*p == IAC) {
			got_iac = 1;
			break;
		}
		p++;
		size++;
	}
	if(size > 0)
		write(fdout, p2, size);
	if(got_iac) {
		TelOpts = 1;
		p++;
	}
   }
}

int tel_out(fdout, buf, size)
int fdout;
char *buf;
int size;
{
char *p;
int got_iac, len;

   p = buf;
   while(size > 0) {
	buf = p;
	got_iac = 0;
	if((p = (char *)memchr(buf, IAC, size)) != (char *)NULL) {
		got_iac = 1;
		p++;
	} else
		p = buf + size;
	len = p - buf;
	if(len > 0)
		(void) write(fdout, buf, len);
	if(got_iac)
		(void) write(fdout, p - 1, 1);
	size = size - len;
   }
}

static int DoTelOpt(fdout, c)
int fdout;
int c;
{
   if(TelOpts == 1) {
	switch(c) {
		case WILL:
		case WONT:
		case DO:
		case DONT:
			ThisOpt = c;
			TelOpts++;
			break;
		case IAC:
			write(fdout, &c, 1);
			TelOpts = 0;
			break;
		default:
			TelOpts = 0;
	}
	return(TelOpts);
   }
   switch(ThisOpt) {
	case WILL:
		dowill(c);
		break;
	case WONT:
		dowont(c);
		break;
	case DO:
		dodo(c);
		break;
	case DONT:
		dodont(c);
		break;
	default:
		TelOpts = 0;
		return(0);
   }
   
   TelOpts = 0;
   return(1);
}

static void dowill(c)
int c;
{
int ack;

   switch(c) {
	case TELOPT_BINARY:
	case TELOPT_ECHO:
	case TELOPT_SGA:
		if(TelROpts[c] == 1)
			return;
		TelROpts[c] = 1;
		ack = DO;
		break;
	default:
		ack = DONT;
   }
   respond(ack, c);
}

static void dowont(c)
int c;
{
   if(c <= LASTTELOPT) {
	if(TelROpts[c] == 0)
		return;
	TelROpts[c] = 0;
   }
   respond(DONT, c);
}

static void dodo(c)
int c;
{
int ack;

   switch(c) {
	default:
		ack = WONT;
   }
   respond(ack, c);
}

static void dodont(c)
int c;
{
   if(c <= LASTTELOPT) {
	if(TelLOpts[c] == 0)
		return;
	TelLOpts[c] = 0;
   }
   respond(WONT, c);
}

static void respond(ack, option)
int ack, option;
{
unsigned char c[3];

   c[0] = IAC;
   c[1] = ack;
   c[2] = option;
   /* write(telfdout, c, 3); */
}
