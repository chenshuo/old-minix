struct rgb_value {
   unsigned char r;
   unsigned char g;
   unsigned char b;
};

struct scr_attr {
  long addr;	/* location of the bitmap */
  int width;	/* size is 0 .. width - 1 pixels */
  int heigth;	/* size is 0 .. heigth - 1 pixels*/
  int planes;	/* number of planes */
  unsigned short clut_size;
  unsigned short nr_color_bits;
};

struct clut_entry {
  int index;
  struct rgb_value rgb;
};

/* offset for the SCREEN minor numbers */
#define SCREEN_DEV 64

