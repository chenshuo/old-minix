#ifndef TYPE_H
#define TYPE_H

struct tasktab {
  _PROTOTYPE( void (*initial_pc), (void) );
  int stksize;
  char name[8];
};

typedef _PROTOTYPE( int (*rdwt_t), (message *m_ptr) );
typedef _PROTOTYPE( void (*watchdog_t), (void) );

#if (CHIP == INTEL)
typedef unsigned port_t;
typedef unsigned segm_t;

/* The register type is usually the natural 'unsigned', but not during 386
 * initialization, when it has to be unsigned long!
 */
#if INTEL_32BITS
typedef u32_t reg_t;		/* machine register */
#else
typedef u16_t reg_t;
#endif

/* The stack frame layout is determined by the software, but for efficiency
 * it is laid out so the assembly code to use it is as simple as possible.
 * 80286 protected mode and all real modes use the same frame, built with
 * 16-bit registers.  Real mode lacks an automatic stack switch, so little
 * is lost by using the 286 frame for it.  The 386 frame differs only in
 * having 32-bit registers and more segment registers.  The same names are
 * used for the larger registers to avoid differences in the code.
 */
struct stackframe_s {
#if INTEL_32BITS
  u16_t gs;
  u16_t fs;
#endif
  u16_t es;
  u16_t ds;
  reg_t di;			/* di through cx are not accessed in C */
  reg_t si;			/* order is to match pusha/popa */
  reg_t fp;			/* bp */
  reg_t st;			/* hole for another copy of sp */
  reg_t bx;
  reg_t dx;
  reg_t cx;
  reg_t retreg;			/* ax */
  reg_t retadr;			/* return address for assembly code save() */
  reg_t pc;			/* interrupt pushes rest of frame */
  reg_t cs;
  reg_t psw;
  reg_t sp;
  reg_t ss;
};

struct segdesc_s {		/* segment descriptor for protected mode */
  u16_t limit_low;
  u16_t base_low;
  u8_t base_middle;
  u8_t access;			/* |P|DL|1|X|E|R|A| */
#if INTEL_32BITS
  u8_t granularity;		/* |G|X|0|A|LIMT| */
  u8_t base_high;
#else
  u16_t reserved;
#endif
};

struct farptr_s {		/* far pointer for debugger hooks */
  reg_t offset;
  u16_t selector;
#if INTEL_32BITS
  u16_t pad;
#endif
};
#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
typedef _PROTOTYPE( void (*dmaint_t), (void) );

typedef u32_t reg_t;		/* machine register */

/* The name and fields of this struct were chosen for PC compatibility. */
struct stackframe_s {
  reg_t retreg;			/* d0 */
  reg_t d1;
  reg_t d2;
  reg_t d3;
  reg_t d4;
  reg_t d5;
  reg_t d6;
  reg_t d7;
  reg_t a0;
  reg_t a1;
  reg_t a2;
  reg_t a3;
  reg_t a4;
  reg_t a5;
  reg_t fp;			/* also known as a6 */
  reg_t sp;			/* also known as a7 */
  reg_t pc;
  u16_t psw;
  u16_t dummy;			/* make size multiple of reg_t for system.c */
};

struct fsave {
  struct cpu_state {
	u16_t i_format;
	u32_t i_addr;
	u16_t i_state[4];
  } cpu_state;
  struct state_frame {
	u8_t frame_type;
	u8_t frame_size;
	u16_t reserved;
	u8_t frame[212];
  } state_frame;
  struct fpp_model {
	u32_t fpcr;
	u32_t fpsr;
	u32_t fpiar;
	struct fpN {
		u32_t high;
		u32_t low;
		u32_t mid;
	} fpN[8];
  } fpp_model;
};
#endif /* (CHIP == M68000) */

#endif /* TYPE_H */
