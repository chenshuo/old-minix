/* This file contains some dumping routines for debugging. */

#include "kernel.h"
#include <minix/com.h>
#include "proc.h"
#include "tty.h"

#define NSIZE 40

char aout[NR_PROCS][NSIZE + 1];	/* pointers to the program names */
char *vargv;

FORWARD _PROTOTYPE(void prname, (int i));

/*===========================================================================*
 *				DEBUG routines here			     *
 *===========================================================================*/
#if (CHIP == INTEL)
PUBLIC void p_dmp()
{
/* Proc table dump */

  register struct proc *rp;
  phys_clicks base, size;
  int index;

  printf(
         "\r\nproc  --pid -pc- -sp flag -user- --sys-- -base- -size-  recv- command\r\n");

  for (rp = BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
	if (rp->p_flags & P_SLOT_FREE) continue;
	base = rp->p_map[T].mem_phys;
	size = rp->p_map[S].mem_phys + rp->p_map[S].mem_len - base;
	prname(proc_number(rp));
	printf("%5u %4lx %4lx %2x %7U %7U %5uK %5uK  ",
	       rp->p_pid,
	       (unsigned long) rp->p_reg.pc,
	       (unsigned long) rp->p_reg.sp,
	       rp->p_flags,
	       rp->user_time, rp->sys_time,
	       click_to_round_k(base), click_to_round_k(size));
	index = proc_number(rp);
	if (index > LOW_USER) {
		printf("%s", aout[index]);
	}
	printf("\r\n");
  }
  printf("\r\n");
}

#endif				/* (CHIP == INTEL) */

#if (SHADOWING == 0)

PUBLIC void map_dmp()
{
  register struct proc *rp;
  phys_clicks base, size;

  printf("\r\nPROC   -----TEXT-----  -----DATA-----  ----STACK-----  -BASE- -SIZE-\r\n");
  for (rp = BEG_SERV_ADDR; rp < END_PROC_ADDR; rp++) {
	if (rp->p_flags & P_SLOT_FREE) continue;
	base = rp->p_map[T].mem_phys;
	size = rp->p_map[S].mem_phys + rp->p_map[S].mem_len - base;
	prname(proc_number(rp));
	printf(" %4x %4x %4x  %4x %4x %4x  %4x %4x %4x  %5uK %5uK\r\n",
	       rp->p_map[T].mem_vir, rp->p_map[T].mem_phys, rp->p_map[T].mem_len,
	       rp->p_map[D].mem_vir, rp->p_map[D].mem_phys, rp->p_map[D].mem_len,
	       rp->p_map[S].mem_vir, rp->p_map[S].mem_phys, rp->p_map[S].mem_len,
	       click_to_round_k(base), click_to_round_k(size));
  }
}

#else

PUBLIC void map_dmp()
{
  register struct proc *rp;
  vir_clicks base, limit;

  printf("\r\nPROC   --TEXT---  --DATA---  --STACK-- SHADOW FLIP P BASE  SIZE\r\n");
  for (rp = BEG_SERV_ADDR; rp < END_PROC_ADDR; rp++) {
	if (rp->p_flags & P_SLOT_FREE) continue;
	base = rp->p_map[T].mem_phys;
	limit = rp->p_map[S].mem_phys + rp->p_map[S].mem_len;
	prname(proc_number(rp));
	printf(" %4x %4x  %4x %4x  %4x %4x   %4x %4d %d %4uK %4uK\r\n",
	       rp->p_map[T].mem_phys, rp->p_map[T].mem_len,
	       rp->p_map[D].mem_phys, rp->p_map[D].mem_len,
	       rp->p_map[S].mem_phys, rp->p_map[S].mem_len,
	       rp->p_shadow, rp->p_nflips, rp->p_physio,
	       click_to_round_k(base), click_to_round_k(limit));
  }
}

#endif

#if (CHIP == M68000)
PUBLIC void p_dmp()
{
/* Proc table dump */

  register struct proc *rp;
  vir_clicks base, limit;
  int index;

  printf(
         "\r\nproc    pid     pc     sp  splow flag  user    sys   recv  command\r\n");

  for (rp = BEG_PROC_ADDR; rp < END_PROC_ADDR; rp++) {
	if (rp->p_flags & P_SLOT_FREE) continue;
	base = rp->p_map[T].mem_phys;
	limit = rp->p_map[S].mem_phys + rp->p_map[S].mem_len;
	prname(proc_number(rp));
	printf(" %4u %6lx %6lx %6lx %4x %5U %6U   ",
	       rp->p_pid,
	       (unsigned long) rp->p_reg.pc,
	       (unsigned long) rp->p_reg.sp,
	       (unsigned long) rp->p_splow,
	       rp->p_flags,
	       rp->user_time, rp->sys_time);
	if (rp->p_flags == 0)
		printf("      ");
	else {
		if (rp->p_flags & RECEIVING) prname(rp->p_getfrom);
		if (rp->p_flags & SENDING) {
			printf("S: ");
			prname(rp->p_sendto);
		}
	}

	index = proc_number(rp);
	if (index > LOW_USER) {
		printf("%s", aout[index]);
	}
	printf("\r\n");
  }
  printf("\r\n");
}


PUBLIC void reg_dmp(rp)
struct proc *rp;
{
  register int i;
  static char *regs[NR_REGS] = {
		    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
		    "a0", "a1", "a2", "a3", "a4", "a5", "a6"
  };
  reg_t *regptr = (reg_t *) & rp->p_reg;

  printf("reg = %08lx, ", rp);
  printf("ksp = %08lx\r\n", (long) &rp + sizeof(rp));
  printf(" pc = %08lx, ", rp->p_reg.pc);
  printf(" sr =     %04x, ", rp->p_reg.psw);
  printf("trp =       %2x\r\n", rp->p_trap);
  for (i = 0; i < NR_REGS; i++) 
	printf("%3s = %08lx%s",regs[i], *regptr++, (i&3) == 3 ? "\r\n" : ", ");
  printf(" a7 = %08lx\r\n", rp->p_reg.sp);
#if (SHADOWING == 1)
    mem_dmp((char *) (((long) rp->p_reg.pc & ~31L) - 96), 128);
    mem_dmp((char *) (((long) rp->p_reg.sp & ~31L) - 32), 256);
#else
    mem_dmp((char *) (((long) rp->p_reg.pc & ~31L) - 96 +
			((long)rp->p_map[T].mem_phys<<CLICK_SHIFT)), 128);
    mem_dmp((char *) (((long) rp->p_reg.sp & ~31L) - 32 +
			((long)rp->p_map[S].mem_phys<<CLICK_SHIFT)), 256);
#endif
}

#endif				/* (CHIP == M68000) */


PUBLIC void mem_dmp(adr, len)
char *adr;
int len;
{
  register i;
  register long *p;

  for (i = 0, p = (long *) adr; i < len; i += 4) {
#if (CHIP == M68000)
	if ((i & 31) == 0) printf("\r\n%lX:", p);
	printf(" %8lX", *p++);
#else
	if ((i & 31) == 0) printf("\r\n%X:", p);
	printf(" %8X", *p++);
#endif /* (CHIP == M68000) */
  }
  printf("\r\n");
}


PUBLIC void tty_dmp()
{
  struct tty_struct *tp;
  int i;

  printf("tty_events = %u\r\n", tty_events);

  for (i = 0; i < NR_CONS + NR_RS_LINES; i++) {
	tp = &tty_struct[i];
	printf("line %d; incount = %d, inleft = %d, outleft = %d\n",
	       i, tp->tty_incount, tp->tty_inleft, tp->tty_outleft);
	if (tp->tty_busy)
		printf("busy\r\n");
	else
		printf("not busy\r\n");
	if (tp->tty_inhibited)
		printf("inhibited\r\n");
	else
		printf("not inhibited\r\n");
	if (tp->tty_waiting)
		if (tp->tty_waiting == SUSPENDED)
			printf("suspended\r\n");
		else
			printf("waiting\r\n");
	else
		printf("not waiting\r\n");
  }
}


PRIVATE void prname(i)
int i;
{
  if (i == ANY)
	printf("ANY   ");
  else if ((unsigned) (i + NR_TASKS) <= LOW_USER + NR_TASKS)
	printf("%s", tasktab[i + NR_TASKS].name);
  else
	printf("%4d  ", i);
}


PUBLIC void set_name(source_nr, proc_nr, ptr)
int source_nr, proc_nr;
char *ptr;
{
/* When an EXEC call is done, the kernel is told about the stack pointer.
 * It uses the stack pointer to find the command line, for dumping
 * purposes.
 */

  phys_bytes src, dst;
  char *np;

  aout[proc_nr][0] = 0;
  if (!ptr) return;

  src = numap(source_nr, (vir_bytes) ptr, (vir_bytes) NSIZE);
  if (!src) return;
  dst = umap(proc_addr(SYSTASK), D, (vir_bytes) (aout[proc_nr]), (vir_bytes)NSIZE);
  phys_copy(src, dst, (phys_bytes) NSIZE);

  aout[proc_nr][NSIZE] = 0;
  for (np = &aout[proc_nr][0]; np < &aout[proc_nr][NSIZE]; np++)
	if (*np <= ' ' || *np >= 0177) *np = 0;
}


PUBLIC void fork_name(p1, p2)
int p1, p2;
{
  memcpy(aout[p2], aout[p1], NSIZE + 1);
}
