#if (MACHINE != ATARI)
#error this is the ATARI version
#endif

/* Kludge for MINIX 1.5.10 */
#ifndef ATARI_TYPE
#define ATARI_TYPE	  ST
#define ST		   1	/* all ST's and Mega ST's */
#define STE		   2	/* all STe and Mega STe's */
#define TT		   3
#endif

#define SCR_ADDR	(long)0

struct video_mode {
  struct scr_attr attr;
  short mode;	/* ST/TT: video mode */
};

struct video_mode video_mode[] =
{
/*
 *   +--------------------------------------- video memory address
 *   |           +--------------------------- pixels per line
 *   |           |    +---------------------- pixels per row
 *   |           |    |   +------------------ number of planes
 *   |           |    |   |    +------------- size of clut
 *   |           |    |   |    |   +--------- bits per color
 *   |           |    |   |    |   |     +--- video mode
 *   |           |    |   |    |   |     |
 *   V           V    V   V    V   V     V
 */
 #if (ATARI_TYPE == ST)
 { { SCR_ADDR,  320, 200, 4,  16,  3 }, 0x0  , },	/* ST lo res */
 { { SCR_ADDR,  640, 200, 2,   4,  3 }, 0x1  , },	/* ST med res */
 { { SCR_ADDR,  640, 400, 1,   1,  1 }, 0x2  , },	/* ST hi res */
 #endif
 #if (ATARI_TYPE == STe)
 { { SCR_ADDR,  320, 200, 4,  16,  4 }, 0x0  , },	/* STe lo res */
 { { SCR_ADDR,  640, 200, 2,   4,  4 }, 0x1  , },	/* STe med res */
 { { SCR_ADDR,  640, 400, 1,   1,  4 }, 0x2  , },	/* STe hi res */
 #endif
 #if (ATARI_TYPE == TT)
 { { SCR_ADDR,  320, 200, 4,  16,  4 }, 0x000, },	/* ST lo res */
 { { SCR_ADDR,  640, 200, 2,   4,  4 }, 0x100, },	/* ST med res */
 { { SCR_ADDR,  640, 400, 1,   1,  4 }, 0x200, },	/* ST hi res */
 { { SCR_ADDR,  320, 480, 8, 256,  4 }, 0x700, },	/* TT lo res */
 { { SCR_ADDR,  640, 480, 4,  16,  4 }, 0x400, },	/* TT med res */
 { { SCR_ADDR, 1280, 480, 1,   1,  1 }, 0x600, },	/* TT hi res */
 #endif
};
