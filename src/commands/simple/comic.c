#define VERSION "2.5"
/*
   This is COMIC (CO_mpress MI_nix C). It was written (p)
   by Jan-Mark Wams (email: jms@cs.vu.nl) in 1991.
  
   `Comic' is tuned to compress the minix source (kernel, fs, mm, commands).
   The algorithm replaces repeated strings with an <offset,length> pair.
   The longer the repeated string, the better the compression. If the text 
   contains a lot of short repeated strings, (eg. font-files) use an other 
   compression method like LZW (ie. ``compress'').
  
   It should compile under MINIX, UNIX, MS-DOS, etc. But if you have
   to make any chainges for a new OS. (ie. if you port it.) PLEASE
   let me know.  (email: jms@cs.vu.nl).
  
   Define one of following:
        _MINIX, AMOEBA, UNIX, DOS, C89, DOS, MSC, GCC, M68
  
   Compiler Defines it checks for:
        _POSIX_SOURCE, __BCC__, __TURBOC__, __STDC__
  
   Extra options:
        H1_SIZE=2^n, H2_SIZE=2^n, LINT, DEBUG, NDEBUG
   Don't define H2_SIZE without defining H1_SIZE. On big boxes, use
   256 for both, this will give you a speed up.
  
   The file format is as follows. (Note in version 2.0 N is always zero).
  
   name                 size            index   remark
  ---------------------------------------------------------------------
   MAGIC                1 byte          0       0x69
   MAGIC and FLAGS      1 byte          1       (0x60 | flags)
   optional FLAGS       N bytes         2       Encription etc.
   optional DOS SUFFIX  2 bytes         2+N     Original dos suffix.
   optional DATA        M > 0 bytes     4+N     Compressed original.
  
   History:
    VERSION |  USER  |  WHAT
      2.0      jms      Made one source file version.
      2.1      jms      Made compilable under gcc, turbo-C++ 1.0, ncc etc.
      2.2      jms      Speedup in Eqlen(). Newstyle layout.
      2.3      jms      Added ``0xFF & (int)'' twice in output_pair().
      2.4      jms	Added stack hog in hash_init().
      2.5      jms	s/MINIX/_MINIX/g
*/

/* General includes and defines.
*/
#ifdef DOS
#  include <fcntl.h>    /* Use O_BINARY by `setmode()' in SM-DOS. */
#  include <io.h>       /* For `setmode()' to. */
# ifdef MSC
#  include <stdlib.h>
# endif
#endif
#if __STDC__ && ! GCC
# include <stdlib.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>             /* Included for strxxx(). */
#include <ctype.h>              /* For isupper() and tolower(). */
#include <sys/types.h>          /* Included for stat(). */
#include <sys/stat.h>           /* Idem. */
#include <unistd.h>

/*
   The S_ISREG works with Turbo C! Isn't life beautiful?
*/
#ifndef S_ISREG 
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#endif

#ifdef DOS
# define TTY    "CON"           /* The terminal device under MS-DOS. */
#else   /* DOS */
# define TTY    "/dev/tty"
#endif /* DOS */

# define next_arg()\
    (argc-- <= 0 ? NULL : *(argv++))

#define str_equel(s1,s2)    (strcmp(s1, s2) == 0)

#define NAMELEN_MAX     128

char *COMIC      = "comic";
char *XCAT       = "xcat";
char *DECOMIC    = "decomic";
char *STDIN      = "stdin"; /* Name of the current std* files. */
char *STDOUT     = "stdout";

char *SUFFIX     = "-X"; /* Suffix for comiced files. */
#define SUFFIX_LEN 2     /* Change SUFFIX and SUFFIX_LEN together. */

/* Compress constants.
*/
#define HUFFMAN_BITS    8       /* Huffman codes are 8 bits. */
#define LENGTH_BITS     8       /* # of bits in length info. */
#define OFFSET_BITS     13      /* # of bits in offset info. */
#define PAIR_MAX        1       /* There are two pairs. */
#define HUFFMAN_SIZE    256     /* Max for 8 bits. */
#define LENGTH_SIZE     256     /* Max for 8 bits. */
#define OFFSET_SIZE     8192    /* 13 Bits. OFFSET_SIZE > LENGHT_SIZE * 2 */
#define OFFSET_MIN      1       /* Minimum offset. */
#define LENGTH_MIN      2       /* Minimum length. */
#define OFFSET_MAX      (OFFSET_SIZE + OFFSET_MIN - 1)
#define LENGTH_MAX      (LENGTH_SIZE + LENGTH_MIN - 1)
#define INPUT_SIZE      (LENGTH_SIZE + PAIR_MAX + LENGTH_MIN)
#define BUFF_SIZE       (INPUT_SIZE + OFFSET_SIZE + OFFSET_MIN)
#define BUFF_MAX        (BUFF_SIZE - 1)
#define NON_INPUT_SIZE  (BUFF_SIZE - INPUT_SIZE)
#define LOW_OFFSET_BITS (OFFSET_BITS - HUFFMAN_BITS)

/* To shut up lint, a bit ;-(
*/
#ifdef LINT
extern char *memset();
extern void *malloc();
#endif

/* Global variables.
*/
char *pname;                    /* Name of program. */
char suffix[SUFFIX_LEN];       /* Last two DOS suffix characters. */

/* Pairs are built of a start pointer and a length pointer.
*/
typedef struct pair {           /* Part descriptor type. */
        char * start;           /* Pointer in the circular buffer. */
        int length;             /* Length of part. */
} pair_t, *pair_p;

pair_t pairs[2];                /* Buffer for pair descriptors. */
pair_p pair_0 = &(pairs[0]);    /* Short hands. */
pair_p pair_1 = &(pairs[1]);

char Buff[BUFF_SIZE + 1];       /* The Buffer. Add one for Sentinel. */
char *Bend;                     /* Pointer to the last char in Buff. */
#define Sentp (Bend + 1)	/* First char after buffer. */
char *Bp = Buff;                /* Pointer to unprocessed char. */
char *Binputend = Buff;         /* End of read ahead input. */
int  Bsize;

long Nin = 0, Nout = 0;         /* Total # of bytes read/written. */

int v_flag = 0;                 /* Verbose flag given. */
int c_flag = 0;                 /* Cat (output to stdout) flag. */
int f_flag = 0;                 /* Force flag. */
int d_flag = 0;                 /* Decomic flag. */
int r_flag = 0;                 /* Raw flag, no header, only data. */
#ifdef DOS
int s_flag = 1;                 /* Save suffix MS-DOS like. */
int a_flag = 0;                 /* Ascii option, cast \r\n to \n. */
#else
int s_flag = 0;                 /* No MS-DOS, no default suffix savage. */
#endif

#ifdef __TURBOC__
extern unsigned _stklen = 256;
#endif

#ifdef DOS
# define str_2_upper(s) (void)strupr(s)
# define str_2_lower(s) (void)strlwr(s)
#endif /* DOS */

/* Index type is used for indexing the buffer and the hash tables.
*/
typedef unsigned short indext;
#define INDEX_END       ((indext)BUFF_SIZE + 1)

/* Buffer macros.
*/
#define Bsucc(p) ((p) == Bend ? Buff : (p) + 1)
#define Bpred(p) ((p) == Buff ? Bend : (p) - 1)
#define Badd(p, n) ((p) + (n) - ((p) + (n) > Bend ? Bsize : 0))
#define Bsub(p, n) ((p) - (n) + ((p) < Buff + (n) ? Bsize : 0))
#define Bdelta(p, q) (int)((p) - (q) + ((p) < (q) ? Bsize : 0))
#define Blookupstart() Bsub(Bp, NON_INPUT_SIZE)
#define Bindex(p) (indext)((p) - Buff)
#define Binit() (void)memset (Buff, '\0', Bsize)

/*
   Pre filling the buffer might give better compression, but
   would enlarge the compress program. '-) Perhaps a pseudo
   random generator, filling the buffer with the most likely
   (lowest number of bits in their huffman code) characters.
   Note: the buffer (Buff) must be filled with something.
*/

/* 
   If H1_SIZE and H2_SIZE are defined on compile time, the hash 
   functions might be quicker on some compilers if not the maximum
   values for H1_SIZE and H2_SIZE will be found at run time.
*/
#ifndef H2_SIZE
int H1_SIZE = 256;   /* Max value for SIZE = 256. */
int H2_SIZE = 256;
#endif

/* Define Hash functions.
*/
#define H1(i) ((indext)((i) & (H1_SIZE -1)))
#define H2(i) ((indext)((i) & (H2_SIZE -1)))

/* The buffer index macros.
*/
#define Ipred(i) (((i) == 0 ? BUFF_SIZE : (i)) - 1)
#define Isucc(i) ((i) == BUFF_SIZE - 1 ? 0 : (i) + 1)
#define hash_entry(i,j) hash_tbl[H1(i) + H1_SIZE * H2(j)]

/* 
   About magic; The first byte is a normal magic byte. The second byte 
   is 0x6X, where the X nybble contains extra info.
*/

#define MAGIC1          (char)0x69      /* Magic number. */
#define MAGIC2          (char)0x60      /* High part of 2d byte. */

#define STATIC_BIT      (char)0x8       /* Use builtin Huffman tables. */
#define DYNAMIC_BIT     (char)0x4       /* Huffman tables precede data. */
#define SUFFIX_BIT      (char)0x2       /* Next two bytes are old suffix. */
#define EXTEND_BIT      (char)0x1       /* More header data. */

/*
   Third byte indicates extra data-processing. To allow a third byte
   the EXTEND_BIT has to been set in the second header byte. This leaves
   room for futer expansions like encription etc. This will make the
   current version signal something is wrong incase it gets data from
   a new version jet to come.
*/

#define EXTRA_EXTEND    (char)0x0       /* Extra extension bytes. */
#define TRANSLATE       (char)0x1       /* Input was xlated. */

/* Huffman types.
*/
typedef long bitstr_t;

typedef struct tree_s {
        int _0, _1;
} tree_t[];

typedef struct ctab_s {
        bitstr_t code;
	char     length; /* Has to hold up to sizeof(bitstr_t) * 8 bits. */
} ctab_t[];

#define Hchar 0
#define Hoff 1
#define Hlen 2

/* 
   There is a big redundancy in the defined tables: both encode and 
   decode tables are included. (The bit length per code would be 
   enough but this would mean timeconsuming run time generation.) Ctab 
   tables hold <bitpattern,length> pairs. Tree tables hold a binary 
   decsent tree with per node a 0 and a 1 side.
*/

/* Huffman Tables these are generated by comicgen.
*/
ctab_t ctab_char = {
        {0x54L,9},{0x3B6L,10},{0x67AL,11},{0x242L,10},
        {0x33DCL,14},{0xD99L,13},{0xD98L,13},{0x76BL,11},
        {0xCF6L,12},{0x17L,5},{0x2CL,6},{0x1977L,14},
        {0xCF749L,20},{0x1B6L,10},{0xAAL,10},{0x13BL,9},
        {0x6DEL,12},{0xCF748L,20},{0x6CFL,12},{0x1976L,14},
        {0x1975L,14},{0xD97L,13},{0x76AL,11},{0xB4DL,12},
        {0xD96L,13},{0xB4CL,12},{0x1974L,14},{0x1973L,14},
        {0xB4BL,12},{0xB4AL,12},{0xB49L,12},{0x1972L,14},
        {0x8L,5},{0x4FL,7},{0x1CL,6},{0x120L,9},
        {0x3B4L,10},{0x31L,8},{0x75L,7},{0x28L,6},
        {0x1AL,5},{0x10L,5},{0x1CL,7},{0x30L,8},
        {0x3L,6},{0x26L,6},{0x31L,6},{0x3AL,7},
        {0x15L,6},{0x2L,6},{0xBL,6},{0x1L,7},
        {0x3EL,7},{0x54L,7},{0x74L,7},{0x61L,7},
        {0x55L,7},{0x13L,8},{0x1DL,7},{0xFL,6},
        {0x77L,7},{0x29L,7},{0x56L,7},{0x38L,9},
        {0x12L,8},{0x14L,7},{0x2BL,8},{0x2FL,7},
        {0x6EL,7},{0x13L,7},{0x66L,7},{0x51L,8},
        {0xB5L,8},{0x45L,7},{0x19FL,9},{0xA1L,9},
        {0x37L,7},{0x6FL,7},{0x57L,7},{0x3DL,7},
        {0x60L,7},{0x156L,11},{0x44L,7},{0xFL,7},
        {0x2EL,7},{0x5BL,7},{0xC9L,9},{0x9CL,8},
        {0xDAL,9},{0xC8L,9},{0x13AL,9},{0x3CL,7},
        {0x39L,9},{0x19L,7},{0x33CL,10},{0x36L,6},
        {0xD95L,13},{0xDL,6},{0x32L,6},{0x1CL,5},
        {0x8L,6},{0x1FL,5},{0x16L,6},{0x8L,7},
        {0x33L,7},{0x6L,6},{0xCEL,8},{0x3BL,7},
        {0x25L,6},{0x23L,6},{0x5L,6},{0x1L,6},
        {0x18L,6},{0x168L,9},{0x1AL,6},{0x9L,5},
        {0x1EL,5},{0x29L,6},{0x3FL,7},{0x12L,7},
	{0x1DL,8},{0,7},{0xECL,8},{0x91L,8},
        {0xA0L,9},{0x49L,7},{0x5A7L,11},{0xCF747L,20},
        {0xB48L,12},{0xCF746L,20},{0x1971L,14},{0xCF745L,20},
        {0x1970L,14},{0xCF744L,20},{0x196FL,14},{0xCF743L,20},
        {0xD94L,13},{0xCF742L,20},{0xCF741L,20},{0xCF740L,20},
        {0x67BBFL,19},{0x196EL,14},{0x67BBEL,19},{0x67BBDL,19},
        {0x196DL,14},{0x67BBCL,19},{0x67BBBL,19},{0x67BBAL,19},
        {0xD93L,13},{0x67BB9L,19},{0x196CL,14},{0x67BB8L,19},
        {0x67BB7L,19},{0x196BL,14},{0xD92L,13},{0x196AL,14},
        {0x1969L,14},{0x1968L,14},{0x1967L,14},{0x1966L,14},
        {0x1965L,14},{0xD91L,13},{0xD90L,13},{0xD8FL,13},
        {0x3B7L,10},{0xD8EL,13},{0xD8DL,13},{0x1964L,14},
        {0x1963L,14},{0xD8CL,13},{0x1962L,14},{0xD8BL,13},
        {0x1961L,14},{0xD8AL,13},{0x1960L,14},{0x195FL,14},
        {0x90FL,12},{0xD89L,13},{0x195EL,14},{0xD88L,13},
        {0x195DL,14},{0x90EL,12},{0x195CL,14},{0xD87L,13},
        {0x195BL,14},{0x195AL,14},{0x1959L,14},{0xD86L,13},
        {0x67BB6L,19},{0xD85L,13},{0x67BB5L,19},{0x1958L,14},
        {0x1957L,14},{0x36EL,11},{0xD84L,13},{0x1956L,14},
        {0x1955L,14},{0xD83L,13},{0x1954L,14},{0x1953L,14},
        {0x67BB4L,19},{0x1952L,14},{0x1951L,14},{0x1950L,14},
        {0x194FL,14},{0x194EL,14},{0x67BB3L,19},{0x194DL,14},
        {0x67BB2L,19},{0x194CL,14},{0x67BB1L,19},{0xD82L,13},
        {0x67BB0L,19},{0x194BL,14},{0x67BAFL,19},{0x194AL,14},
        {0x67BAEL,19},{0x1949L,14},{0x67BADL,19},{0x1948L,14},
        {0x67BACL,19},{0x1947L,14},{0x67BABL,19},{0x1946L,14},
        {0x67BAAL,19},{0x1945L,14},{0x1944L,14},{0x1943L,14},
        {0x1942L,14},{0x1941L,14},{0x67BA9L,19},{0x1940L,14},
        {0x67BA8L,19},{0xD81L,13},{0x67BA7L,19},{0xD80L,13},
        {0x67BA6L,19},{0x6CEL,12},{0xABFL,14},{0xABEL,14},
        {0xABDL,14},{0x90DL,12},{0xABCL,14},{0x6CDL,12},
        {0x67BA5L,19},{0xCBFL,13},{0xABBL,14},{0xCBEL,13},
        {0xABAL,14},{0xCBDL,13},{0xAB9L,14},{0x90CL,12},
        {0xAB8L,14},{0xCBCL,13},{0x19EFL,13},{0x6DFL,12}
};

tree_t tree_char = {
        {17,12},{129,127},{133,131},{137,135},
        {139,138},{142,140},{145,143},{147,146},
        {151,149},{188,152},{200,190},{208,206},
        {212,210},{216,214},{220,218},{224,222},
        {232,230},{236,234},{256,244},{258,257},
        {260,259},{262,261},{264,263},{266,265},
        {268,267},{270,269},{272,271},{274,273},
        {276,275},{278,277},{280,279},{282,281},
        {284,283},{286,285},{288,287},{290,289},
        {4,291},{19,11},{26,20},{31,27},
        {132,130},{141,134},{150,144},{155,153},
        {157,156},{159,158},{167,160},{170,168},
        {174,172},{178,175},{182,180},{185,184},
        {191,186},{195,192},{198,196},{201,199},
        {203,202},{205,204},{209,207},{215,213},
        {219,217},{223,221},{226,225},{228,227},
        {231,229},{239,238},{242,240},{248,246},
        {252,250},{292,254},{6,5},{24,21},
        {136,96},{154,148},{162,161},{165,163},
        {169,166},{173,171},{179,177},{187,183},
        {194,189},{211,197},{235,233},{247,245},
        {253,249},{294,293},{296,295},{298,297},
        {300,299},{302,301},{304,303},{306,305},
        {308,307},{310,309},{312,311},{314,313},
        {316,315},{318,317},{320,319},{322,321},
        {324,323},{8,325},{25,23},{29,28},
        {128,30},{181,176},{251,241},{16,255},
        {237,18},{326,243},{328,327},{330,329},
        {332,331},{334,333},{336,335},{338,337},
        {340,339},{342,341},{344,343},{346,345},
	{348,347},{350,349},{352,351},{354,353},
	{356,355},{22,7},{2,357},{358,126},
        {360,359},{362,361},{193,363},{365,364},
        {367,366},{369,368},{371,370},{373,372},
        {375,374},{377,376},{379,378},{81,380},
        {1,164},{36,381},{94,382},{384,383},
        {3,385},{13,386},{388,387},{390,389},
        {392,391},{394,393},{14,395},{397,396},
        {398,74},{113,399},{90,15},{35,400},
        {88,401},{403,402},{405,404},{89,86},
        {124,75},{0,406},{63,92},{122,407},
        {106,408},{409,72},{87,410},{411,123},
        {413,412},{415,414},{416,71},{43,37},
        {417,66},{418,120},{64,57},{419,60},
        {54,38},{68,77},{70,420},{80,55},
        {421,85},{62,78},{53,56},{422,33},
        {423,125},{82,73},{52,118},{91,79},
        {47,107},{424,76},{425,104},{84,67},
        {426,61},{42,58},{427,93},{65,428},
        {119,69},{429,83},{103,430},{121,51},
        {432,431},{95,433},{98,434},{435,46},
        {10,436},{438,437},{39,117},{45,439},
        {440,108},{441,109},{443,442},{34,444},
        {114,445},{112,446},{102,447},{448,48},
        {449,59},{450,97},{451,50},{100,452},
        {105,453},{454,110},{49,44},{455,111},
        {116,101},{99,456},{40,457},{459,458},
        {460,9},{462,461},{464,463},{41,465},
        {467,466},{469,468},{471,470},{32,115},
        {473,472},{475,474},{477,476},{479,478},
        {481,480},{483,482},{485,484},{487,486},
        {489,488},{491,490},{493,492},{495,494},
        {497,496},{499,498},{501,500},{503,502},
        {505,504},{507,506},{509,508}
};

ctab_t ctab_len = {
	{0x2L,3},{0,3},{0x3L,3},{0x5L,3},
        {0x6L,3},{0x3L,4},{0x9L,4},{0xFL,4},
        {0x5L,5},{0x1CL,5},{0x8L,6},{0x21L,6},
        {0x3AL,6},{0x13L,7},{0x41L,7},{0x45L,7},
        {0x76L,7},{0x24L,8},{0x8CL,8},{0x88L,8},
        {0x8EL,8},{0xEEL,8},{0x4AL,9},{0x100L,9},
        {0x102L,9},{0x101L,9},{0x11EL,9},{0x206L,10},
        {0x225L,10},{0x207L,10},{0x235L,10},{0x1DFL,9},
        {0x237L,10},{0x47DL,11},{0x3BDL,10},{0x448L,11},
        {0x234L,10},{0x44CL,11},{0x12EL,11},{0x47CL,11},
        {0x46DL,11},{0x258L,12},{0x779L,11},{0x778L,11},
        {0x25EL,12},{0x8D9L,12},{0x259D41L,24},{0x4BEL,13},
        {0x892L,12},{0x8D8L,12},{0x89FL,12},{0x966L,14},
        {0x8FCL,12},{0x4B7L,13},{0x4B6L,13},{0x11FFL,13},
        {0x113DL,13},{0x11FEL,13},{0x259D40L,24},{0x11FDL,13},
        {0x4B5L,13},{0x2275L,14},{0x11FCL,13},{0x259CL,16},
        {0x2274L,14},{0x259D3FL,24},{0x2273L,14},{0x2272L,14},
        {0x2271L,14},{0x2270L,14},{0x226FL,14},{0x4B4L,13},
        {0x226EL,14},{0x226DL,14},{0x259D3EL,24},{0x44D1L,15},
        {0x226CL,14},{0x4B2L,13},{0x113CL,13},{0x113BL,13},
        {0x44D0L,15},{0x449FL,15},{0x449EL,15},{0x11FBL,13},
        {0x259D3DL,24},{0x259D3CL,24},{0x259D3BL,24},{0x259D3AL,24},
        {0x226BL,14},{0x259D39L,24},{0x449DL,15},{0x449CL,15},
        {0x449BL,15},{0x449AL,15},{0x259D38L,24},{0x226AL,14},
        {0x11FAL,13},{0x4499L,15},{0x259D37L,24},{0x259D36L,24},
        {0x4498L,15},{0x2269L,14},{0x259D35L,24},{0x259D34L,24},
        {0x259D33L,24},{0x259D32L,24},{0x259D31L,24},{0x259D30L,24},
        {0x259D2FL,24},{0x259D2EL,24},{0x259D2DL,24},{0x259D2CL,24},
        {0x259D2BL,24},{0x259D2AL,24},{0x259D29L,24},{0x12FFL,15},
        {0x259D28L,24},{0x259D27L,24},{0x259D26L,24},{0x259D25L,24},
        {0x259D24L,24},{0x259D23L,24},{0x259D22L,24},{0x259D21L,24},
        {0x259D20L,24},{0x259D1FL,24},{0x12FEL,15},{0x259D1EL,24},
        {0x259D1DL,24},{0x259D1CL,24},{0x259D1BL,24},{0x259D1AL,24},
        {0x259D19L,24},{0x259D18L,24},{0x259D17L,24},{0x259D16L,24},
        {0x259D15L,24},{0x12FDL,15},{0x259D14L,24},{0x259D13L,24},
        {0x259D12L,24},{0x259D11L,24},{0x259D10L,24},{0x259D0FL,24},
        {0x259D0EL,24},{0x259D0DL,24},{0x259D0CL,24},{0x259D0BL,24},
        {0x259D0AL,24},{0x259D09L,24},{0x259D08L,24},{0x259D07L,24},
        {0x259D06L,24},{0x259D05L,24},{0x12FCL,15},{0x259D04L,24},
        {0x259D03L,24},{0x259D02L,24},{0x259D01L,24},{0x259D00L,24},
        {0x12CEFFL,23},{0x12CEFEL,23},{0x12CEFDL,23},{0x12CEFCL,23},
        {0x12CEFBL,23},{0x12CEFAL,23},{0x12CEF9L,23},{0x12CEF8L,23},
        {0x12CEF7L,23},{0x12CEF6L,23},{0x12CEF5L,23},{0x12CFL,15},
        {0x12CEF4L,23},{0x12CEF3L,23},{0x12CEF2L,23},{0x12CEF1L,23},
        {0x12CEF0L,23},{0x12CEEFL,23},{0x12CEEEL,23},{0x12CEEDL,23},
        {0x12CEECL,23},{0x12CEEBL,23},{0x12CEEAL,23},{0x12CEE9L,23},
        {0x12CEE8L,23},{0x12CEE7L,23},{0x12CEE6L,23},{0x12CEE5L,23},
        {0x12CEE4L,23},{0x12CEE3L,23},{0x12CEE2L,23},{0x12CEE1L,23},
        {0x12CEE0L,23},{0x12CEDFL,23},{0x12CEDEL,23},{0x12CEDDL,23},
        {0x12CEDCL,23},{0x12CEDBL,23},{0x12CEDAL,23},{0x12CED9L,23},
        {0x12CED8L,23},{0x12CED7L,23},{0x12CED6L,23},{0x12CED5L,23},
        {0x12CED4L,23},{0x12CED3L,23},{0x12CED2L,23},{0x12CED1L,23},
        {0x12CED0L,23},{0x12CECFL,23},{0x12CECEL,23},{0x12CECDL,23},
        {0x12CECCL,23},{0x12CECBL,23},{0x12CECAL,23},{0x12CEC9L,23},
        {0x12CEC8L,23},{0x12CEC7L,23},{0x12CEC6L,23},{0x12CEC5L,23},
        {0x12CEC4L,23},{0x12CEC3L,23},{0x12CEC2L,23},{0x12CEC1L,23},
        {0x12CEC0L,23},{0x12CEBFL,23},{0x12CEBEL,23},{0x12CEBDL,23},
        {0x12CEBCL,23},{0x12CEBBL,23},{0x12CEBAL,23},{0x12CEB9L,23},
        {0x12CEB8L,23},{0x12CEB7L,23},{0x12CEB6L,23},{0x12CEB5L,23},
        {0x12CEB4L,23},{0x12CEB3L,23},{0x12CEB2L,23},{0x12CEB1L,23},
        {0x12CEB0L,23},{0x12CEAFL,23},{0x12CEAEL,23},{0x12CEADL,23},
        {0x12CEACL,23},{0x12CEABL,23},{0x12CEAAL,23},{0x12CEA9L,23},
        {0x12CEA8L,23},{0x12CEA7L,23},{0x12CEA6L,23},{0x12CEA5L,23},
        {0x12CEA4L,23},{0x12CEA3L,23},{0x12CEA2L,23},{0x12CEA1L,23}
};

tree_t tree_len = {
        {58,46},{74,65},{85,84},{87,86},
        {94,89},{99,98},{103,102},{105,104},
        {107,106},{109,108},{111,110},{113,112},
        {116,114},{118,117},{120,119},{122,121},
        {124,123},{127,125},{129,128},{131,130},
        {133,132},{135,134},{138,136},{140,139},
        {142,141},{144,143},{146,145},{148,147},
        {150,149},{152,151},{155,153},{157,156},
        {159,158},{161,160},{163,162},{165,164},
        {167,166},{169,168},{172,170},{174,173},
        {176,175},{178,177},{180,179},{182,181},
        {184,183},{186,185},{188,187},{190,189},
        {192,191},{194,193},{196,195},{198,197},
        {200,199},{202,201},{204,203},{206,205},
        {208,207},{210,209},{212,211},{214,213},
        {216,215},{218,217},{220,219},{222,221},
        {224,223},{226,225},{228,227},{230,229},
        {232,231},{234,233},{236,235},{238,237},
        {240,239},{242,241},{244,243},{246,245},
        {248,247},{250,249},{252,251},{254,253},
        {256,255},{258,257},{260,259},{262,261},
        {264,263},{266,265},{268,267},{270,269},
        {272,271},{274,273},{276,275},{278,277},
        {280,279},{282,281},{284,283},{286,285},
        {288,287},{290,289},{292,291},{294,293},
        {296,295},{298,297},{300,299},{302,301},
        {304,303},{306,305},{308,307},{310,309},
        {312,311},{314,313},{316,315},{318,317},
        {320,319},{322,321},{324,323},{326,325},
        {328,327},{330,329},{332,331},{334,333},
        {336,335},{338,337},{340,339},{342,341},
        {344,343},{346,345},{348,347},{350,349},
        {352,351},{354,353},{356,355},{358,357},
        {360,359},{362,361},{364,363},{366,365},
        {368,367},{370,369},{372,371},{374,373},
        {376,375},{378,377},{380,379},{382,381},
        {384,383},{386,385},{388,387},{390,389},
        {392,391},{394,393},{396,395},{398,397},
        {400,399},{402,401},{404,403},{406,405},
        {408,407},{410,409},{412,411},{414,413},
        {63,415},{80,75},{82,81},{91,90},
        {93,92},{100,97},{126,115},{154,137},
        {416,171},{64,61},{67,66},{69,68},
        {72,70},{76,73},{95,88},{417,101},
        {419,418},{421,420},{423,422},{51,424},
        {57,55},{62,59},{96,83},{78,56},
        {425,79},{427,426},{429,428},{431,430},
        {433,432},{47,434},{54,53},{71,60},
        {77,435},{437,436},{52,438},{49,45},
        {439,50},{441,440},{443,442},{48,444},
        {44,445},{447,446},{41,448},{43,42},
        {450,449},{39,33},{451,40},{453,452},
        {37,454},{35,455},{38,456},{458,457},
        {459,34},{461,460},{462,32},{36,30},
        {464,463},{465,28},{27,29},{467,466},
        {468,31},{26,469},{471,470},{473,472},
        {24,474},{23,25},{22,475},{21,476},
        {20,477},{18,478},{19,479},{481,480},
        {17,482},{16,483},{485,484},{486,15},
        {487,14},{488,13},{12,489},{491,490},
        {492,11},{10,493},{9,494},{496,495},
        {497,8},{498,7},{499,6},{500,5},
        {4,501},{502,3},{0,2},{1,503},
        {505,504},{507,506},{509,508}
};

ctab_t ctab_off = {
        {0x7L,3},{0x3L,4},{0xCL,4},{0xAL,5},
        {0x12L,5},{0x1BL,5},{0x8L,6},{0x10L,6},
        {0x1CL,6},{0x1FL,6},{0x28L,6},{0x8L,7},
        {0x35L,6},{0x3L,7},{0xBL,7},{0x2FL,6},
        {0xEL,7},{0x24L,7},{0x26L,7},{0x3AL,7},
        {0x3CL,7},{0x40L,7},{0x4EL,7},{0x45L,7},
        {0x5L,8},{0x57L,7},{0x5AL,7},{0x3L,8},
        {0xCL,8},{0x2AL,8},{0x1FL,8},{0x2L,8},
        {0xAL,8},{0x4CL,7},{0x9L,8},{0x2CL,8},
        {0x4FL,8},{0x19L,8},{0x65L,8},{0x89L,8},
        {0x87L,8},{0x29L,8},{0x2FL,8},{0x8DL,8},
        {0x6BL,8},{0x63L,8},{0x6DL,8},{0x5AL,8},
        {0x85L,8},{0x60L,8},{0x5CL,8},{0x9BL,8},
        {0xA5L,8},{0x6FL,8},{0xA4L,8},{0x83L,8},
        {0xACL,8},{0x84L,8},{0x8EL,8},{0x9AL,8},
        {0xD2L,8},{0xBBL,8},{0x2BL,9},{0xABL,8},
        {0x17L,9},{0xBAL,8},{0xAAL,8},{0x35L,9},
        {0xA8L,8},{0x16L,9},{0xB9L,8},{0x8L,9},
        {0x3DL,9},{0x8AL,9},{0x26L,9},{0x9L,9},
	{0xB8L,8},{0x1DL,9},{0,9},{0xB7L,8},
        {0x2AL,9},{0x5DL,9},{0x95L,9},{0x50L,9},
        {0x4AL,9},{0xBFL,9},{0x4FL,9},{0xBBL,9},
        {0x8DL,9},{0x4EL,9},{0x3CL,9},{0x9CL,9},
        {0xCFL,9},{0xC5L,9},{0x57L,9},{0x88L,9},
        {0xD8L,9},{0x37L,9},{0x14FL,9},{0x56L,9},
        {0x94L,9},{0x51L,9},{0xB0L,9},{0xCEL,9},
        {0x5CL,9},{0xF7L,9},{0x97L,9},{0xDDL,9},
        {0xD5L,9},{0xD4L,9},{0x8CL,9},{0xB6L,9},
        {0xF6L,9},{0xD3L,9},{0xBEL,9},{0x118L,9},
        {0x16CL,9},{0xD2L,9},{0xBDL,9},{0x23L,10},
        {0xDCL,9},{0x105L,9},{0x167L,9},{0xF5L,9},
        {0x111L,9},{0x4BL,10},{0xF4L,9},{0xC4L,9},
        {0x13EL,9},{0xC9L,9},{0xCDL,9},{0x4AL,10},
        {0xC3L,9},{0x11FL,9},{0x14EL,9},{0x167L,10},
        {0x1A1L,9},{0x153L,9},{0x119L,9},{0xEDL,9},
        {0x39L,10},{0x9BL,10},{0xD1L,9},{0x163L,9},
        {0xB7L,10},{0x162L,9},{0x52L,10},{0x152L,9},
        {0x51L,10},{0x68L,10},{0x38L,10},{0xBCL,9},
        {0x37L,10},{0x14DL,9},{0x13DL,9},{0x7L,10},
        {0x10CL,9},{0x161L,9},{0x49L,10},{0x6L,10},
        {0x11EL,9},{0x6DL,10},{0x160L,9},{0x1A6L,9},
        {0x104L,9},{0x166L,9},{0x48L,10},{0x1A3L,9},
        {0x14CL,9},{0x15BL,9},{0x165L,9},{0x36L,10},
        {0x63L,10},{0x1A0L,9},{0x5L,10},{0x164L,9},
        {0x93L,10},{0x22L,10},{0x110L,9},{0x6CL,10},
        {0x4L,10},{0x13CL,9},{0x3L,10},{0x3FL,10},
        {0x2L,10},{0x1A2L,9},{0x9AL,10},{0x35L,10},
        {0x116L,10},{0x1A7L,9},{0x13FL,9},{0x50L,10},
        {0x4FL,10},{0x113L,10},{0x12CL,10},{0x92L,10},
        {0x34L,10},{0x175L,10},{0x166L,10},{0x91L,10},
        {0x99L,10},{0x1D8L,10},{0x3EL,10},{0x112L,10},
        {0xB6L,10},{0x190L,10},{0x1A0L,10},{0x4EL,10},
        {0x13AL,10},{0x15AL,9},{0x1B3L,10},{0x21BL,10},
        {0x165L,10},{0x62L,10},{0x98L,10},{0x97L,10},
        {0x21L,10},{0x69L,10},{0x61L,10},{0x164L,10},
        {0x11FL,10},{0x163L,10},{0x174L,10},{0x12DL,10},
        {0x1B2L,10},{0x96L,10},{0x11EL,10},{0x11DL,10},
        {0x1A1L,10},{0x90L,10},{0x199L,10},{0x3DL,10},
        {0xB5L,10},{0x162L,10},{0x1DFL,10},{0x16FL,10},
        {0x185L,10},{0x3CL,10},{0x1DEL,10},{0x11CL,10},
        {0x13BL,10},{0x184L,10},{0x2DBL,10},{0x198L,10},
        {0x21AL,10},{0xB4L,10},{0x2DAL,10},{0x60L,10},
        {0x1DDL,10},{0x1DCL,10},{0x1D9L,10},{0x53L,10},
        {0x117L,10},{0x20L,10},{0x191L,10},{0x16EL,10}
};

tree_t tree_off = {
        {246,242},{244,211},{238,234},{249,248},
        {201,250},{224,210},{206,228},{243,230},
        {205,254},{241,236},{222,197},{255,235},
        {198,135},{219,212},{233,221},{208,240},
        {194,223},{226,220},{239,227},{188,252},
        {203,193},{204,144},{245,232},{186,141},
        {214,200},{225,215},{195,176},{229,199},
        {179,161},{149,217},{213,172},{247,218},
        {146,251},{191,148},{207,192},{131,125},
        {166,158},{202,183},{237,231},{150,140},
        {171,152},{196,187},{177,119},{253,216},
        {159,155},{180,174},{184,182},{163,189},
        {185,167},{173,136},{116,256},{165,122},
        {175,170},{145,143},{162,157},{209,169},
        {147,137},{134,98},{168,153},{128,190},
        {181,154},{160,133},{115,138},{178,124},
        {156,257},{164,121},{112,105},{126,123},
        {259,258},{260,139},{120,107},{96,261},
        {109,108},{117,113},{262,142},{103,92},
        {263,130},{264,129},{127,93},{265,132},
        {114,85},{151,118},{266,87},{111,267},
        {269,268},{102,270},{91,271},{272,106},
        {100,82},{274,273},{110,88},{73,275},
        {95,276},{104,81},{278,277},{99,94},
        {83,101},{89,86},{280,279},{84,281},
        {283,282},{90,72},{284,97},{285,67},
        {287,286},{80,62},{289,288},{74,290},
        {292,291},{294,293},{295,77},{297,296},
        {69,64},{299,298},{71,75},{301,300},
        {78,302},{60,303},{305,304},{65,61},
        {76,70},{306,79},{308,307},{310,309},
        {56,311},{66,63},{68,312},{314,313},
        {54,52},{316,315},{59,51},{58,317},
        {318,43},{319,39},{320,40},{57,48},
        {321,55},{323,322},{325,324},{326,53},
        {327,46},{328,44},{330,329},{332,331},
        {333,38},{334,45},{49,335},{337,336},
        {50,338},{47,339},{341,340},{342,36},
        {344,343},{346,345},{348,347},{349,42},
        {35,350},{29,351},{352,41},{354,353},
        {356,355},{357,30},{359,358},{360,37},
        {362,361},{364,363},{366,365},{28,367},
        {32,368},{369,34},{370,24},{31,27},
        {372,371},{374,373},{376,375},{26,377},
        {379,378},{380,25},{382,381},{384,383},
        {22,385},{33,386},{388,387},{389,23},
        {391,390},{21,392},{20,393},{19,394},
        {396,395},{398,397},{400,399},{402,401},
        {404,403},{406,405},{18,407},{17,408},
        {410,409},{412,411},{414,413},{416,415},
        {16,417},{419,418},{420,14},{11,421},
        {423,422},{425,424},{426,13},{428,427},
        {429,12},{430,15},{432,431},{434,433},
        {10,435},{437,436},{439,438},{441,440},
        {442,9},{8,443},{445,444},{447,446},
        {449,448},{451,450},{7,452},{454,453},
        {6,455},{457,456},{459,458},{461,460},
        {463,462},{464,5},{466,465},{468,467},
        {4,469},{471,470},{473,472},{475,474},
        {3,476},{478,477},{480,479},{482,481},
        {484,483},{2,485},{487,486},{489,488},
        {491,490},{493,492},{494,1},{496,495},
        {497,0},{499,498},{501,500},{503,502},
        {505,504},{507,506},{509,508}
};

/*
   The bits are put on a file, the first bit putted with ``put_bit()''
   is the high bit of the first byte in the file. `Flush_bits()' will 
   add a `1' and pad the byte up to a full byte with zeroes. This will
   make the last bit in the last byte the eof marker. No original length 
   information is stored in the output file.
*/

#define BIT_BUFF_SIZE    1024
char bit_buff[BIT_BUFF_SIZE];
#define eo_buff (bit_buff + BIT_BUFF_SIZE)
char *bit_index = bit_buff;
char *eof_index = bit_buff;
int put_bits = 0;
int put_room;
int get_bits = 0;

#define next()          (bit_index++)            /* The next char. */
#define no_next()       (bit_index == eof_index) /* There is no next. */
#define bit_buff_full() (bit_index == eo_buff)   /* Buffer full. */


_PROTOTYPE(void fatal, (char *failed ));
_PROTOTYPE(void write_bit_buff, (void));
_PROTOTYPE(void read_bit_buff, (void));
_PROTOTYPE(void put_bit, (int bit ));
_PROTOTYPE(int get_bit, (void));
_PROTOTYPE(void init_bits, (void));
_PROTOTYPE(void flush_bits, (void));
_PROTOTYPE(void put_n_bits, (long code, int n ));
_PROTOTYPE(int get_n_bits, (int n ));
_PROTOTYPE(char *basename, (char *name ));
_PROTOTYPE(char *new_name, (char *fname ));
_PROTOTYPE(char *org_name, (char *fname ));
_PROTOTYPE(int new_stdin, (char *newin ));
_PROTOTYPE(int new_stdout, (char *newout ));
_PROTOTYPE(void print_ratio, (void));
_PROTOTYPE(void verbose_info, (void));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void file_start, (void));
_PROTOTYPE(void file_done, (void));
_PROTOTYPE(void hash_init, (void));
_PROTOTYPE(void hash_update, (void));
_PROTOTYPE(void decode_pair, (void));
_PROTOTYPE(void decode_char, (void));
_PROTOTYPE(int get_input, (void));
_PROTOTYPE(int eqlen, (char *p, char *q, int count ));
_PROTOTYPE(void get_token, (int pair_0used ));
_PROTOTYPE(int magic_ok, (void));
_PROTOTYPE(void put_magic, (void));
_PROTOTYPE(int descend, (struct tree_s *tree ));
_PROTOTYPE(void Hinit, (void));
_PROTOTYPE(int Hdecode, (int index ));
_PROTOTYPE(int Hcodelen, (int index, int c ));
_PROTOTYPE(void Hencode, (int index, int i ));
_PROTOTYPE(void decode, (void));
_PROTOTYPE(void output_pair, (int *done ));
_PROTOTYPE(void output_char, (void));
_PROTOTYPE(void encode, (void));
_PROTOTYPE(int main, (int argc, char **argv ));


/* fatal: Print a error on stderr, and exit.
*/
void fatal (failed)
char *failed;
{
  fprintf(stderr, "\r%s: Fatal couldn't %s ", pname, failed);
  if (errno != 0)
        perror("");             /* Prints EOLN to ;-( */
  else
        printf("\n");
  exit(-1);
}


/* write_bit_buff: write the bit i/o buffer to stdout.
*/
void write_bit_buff()
{
  if (write(1, bit_buff, bit_index - bit_buff) != bit_index - bit_buff)
        fatal("write");
  Nout += bit_index - bit_buff;
  bit_index = bit_buff;
}

/* read_bit_buff: read the bit i/o buffer from stdin.
*/
void read_bit_buff()
{
  if ((eof_index = bit_buff + read(0, bit_buff, BIT_BUFF_SIZE)) < bit_buff)
        fatal("read");

  Nin += eof_index - bit_buff;
  bit_index = bit_buff;
}


/* put_bit: put a bit on stdout.
*/
void put_bit(bit)
int bit;
{
  assert(bit == 1 || bit == 0);

  put_bits = (put_bits << 1) | bit;

  if (--put_room == 0) {
        *next() = put_bits;
	if (bit_buff_full()) write_bit_buff();
        put_bits = 0;           /* Flag. */
        put_room = 8;
  }
}


/* get_bit: get a bit from stdin.
*/
int get_bit()
{
  if ((get_bits & 0x7F) == 0) {
        if (get_bits == 0) {    /* First time only. */
                read_bit_buff();
                if (no_next()) {
                        fprintf(stderr, "\r%s: Warning empty file.\n", pname);
                        fflush(stderr);
                        return EOF;
                }
	}                                   
        if (no_next()) return EOF;
	get_bits = (0xFF & (int)*next()) << 1;
        if (no_next()) read_bit_buff();
        if (no_next()) {
                if (get_bits == 0x100)
                        return EOF;     /* Bad luck, think ;-) */
        } else
                get_bits |= 1;  /* Last byte has own EOF marker. */
  } else
        get_bits <<= 1;

  return(get_bits >> 8) & 1;
}

/* Init_bits: Init the bits package.
*/
void init_bits()
{
  bit_index = bit_buff;         /* Not necessary, but nice. */
  eof_index = bit_buff;
  put_bits = 0;                 /* Very necessary. */
  put_room = 8;
  get_bits = 0;
}


/* Flush_bits: Flush the buffered bits to stdout.
*/
void flush_bits()
{
  int i;
  put_bit(1);                            /* Eof marker. */
  for (i = 0; i < 7; i++) put_bit(0);    /* Fillup byte. */
  write_bit_buff();
}

/* put_n_bits: put low `n' bits from code on stream.
*/
void put_n_bits(code, n)
long code;
int n;
{
  int uggy_buggy;       /* The Minix C compiler won't handle longs properly */

  if (n < 0 || n > sizeof(long) * 8)
        n = n;
  assert(n >= 0 && n < sizeof(long) * 8);

  code &= ((long) 1 << n) - 1;
  while (n > put_room) {
        n -= put_room;
	uggy_buggy = (int)(code >> n) & ((1 << put_room) - 1);
        *next() = (put_bits << put_room) | uggy_buggy;
	if (bit_buff_full()) write_bit_buff();
        put_bits = 0;           /* Flag. */
        put_room = 8;
  }
  put_bits = (put_bits << n) | (int)code;
  if ((put_room -= n) == 0) {
        *next() = put_bits;
	if (bit_buff_full()) write_bit_buff();
        put_bits = 0;
        put_room = 8;
  }
}

/* get_n_bits: Get `n' bits from stdin.
*/
int get_n_bits(n)
int n;
{
  int ret = 0;                  /* Return value. */
  int bit;

  assert(n > 0 && n < sizeof(int) * 8);

  while (n-- > 0) {
        ret <<= 1;
        bit = get_bit();
        if (bit == 0) {
                /* EMPTY */
        } else if (bit == 1) {
                ret |= 1;
        } else if (bit == EOF) {
                fprintf(stderr, "\r%s: Unexpected EOF\n", pname);
                exit(-1);
        } else {
                fprintf(stderr, "\r%s: funny bit\n", pname);
                exit(-1);
        }
  }
  return ret;
}

/* Basename: Do a check on name, only comic, decomic and xcat are allowed.
*/
char *basename(name)
char *name;
{
  char *p;
#ifdef DOS
  /* DOS needs special attention. Uppercase letters and leading path
  ** must be removed. Also .com or .exe suffixes have to be removed.
  */
  p = strrchr(name, '\\');
  if (p != NULL)
        name = p + 1;                   /* Kill path prefix. */

  str_2_lower (p);                         /* Make name lower case. */

  p = strrchr(name, '.');
  if (p != NULL) {
	if (!str_equel(p, ".exe") && !str_equel(p, ".com")) {
		fprintf(stderr, "\r%s: Warning funny suffix %s\n", pname, p);
        }
        *p = '\0';                      /* Kill the .exe or .com suffix. */
  }
  else
	fprintf(stderr, "\r%s: Warning no DOS suffix\n", pname);

#else /* Non DOS. */
  p = strrchr(name, '/');
  if (p != NULL)
	name = p + 1;                   /* Kill path prefix. */
#endif
  if (strncmp(name, "[de]comic", 9) == 0) {
#ifdef DOS
	printf("\rHUH?\n");
#else
	printf("\rYou are FUNNY!\n");
#endif
	exit(-99);
  }
  if (!str_equel(name, COMIC)
   && !str_equel(name, DECOMIC)
   && !str_equel(name, XCAT)) {
	fprintf(stderr, "\rThis program must be called [de]comic ");
	fprintf(stderr, "or xcat, not \"%s\".\n", name);
#ifndef DEBUG
        /* In debug or force mode, allow other names.
        */
	if (f_flag < 2) exit(1);
#endif
  }
  return name;
}


/* new_name: generate new output name according to the given name.
*/
char *new_name(fname)
char *fname;
{
  static char ret_name[NAMELEN_MAX];   /* Buffer for return value. */
  char *p;

  /* Check the tail. If the suffix is present, just return it.
  */
  if ((p = strrchr(fname, SUFFIX[0])) != NULL
  && str_equel(p, SUFFIX))
        return fname;

  strncpy(ret_name, fname, NAMELEN_MAX);
  ret_name[(NAMELEN_MAX - 1) - strlen(SUFFIX)] = '\0'; /* Truncate. */

  /* Truncate suffix to be able to hold the SUFFIX. Save the suffix 
  ** for the header. If there is no suffix, add a "." string (DOS).
  */
#ifndef DOS
  if (s_flag) {
#endif
	  if ((p = strrchr(ret_name, '.')) != NULL) {
		if (!f_flag && strlen(p) > 4) {
			fprintf(stderr, "%s: Warning ", pname);
			fprintf(stderr, "suffix %s ", p);
			fprintf(stderr, "will be trucated ");
			p[4] = '\0';
			fprintf(stderr, "to %s\n", p);
                }
                if (!d_flag && s_flag)
			strncpy(suffix, p + 4 - SUFFIX_LEN, SUFFIX_LEN);
		p[4 - SUFFIX_LEN] = '\0';              /* Truncate. */
          }
#ifdef DOS
          else
		strcat(ret_name, ".");
#else
  }
#endif
  strcat(ret_name, SUFFIX);
  return ret_name;
}


/* org_name: generate original name according to the given name.
*/
char *org_name(fname)
char *fname;
{
  static char ret_name[NAMELEN_MAX];   /* Buffer for return value. */
  char *p;

  strncpy(ret_name, fname, NAMELEN_MAX);       /* Make private copy. */
  ret_name[NAMELEN_MAX - 1] = '\0';            /* Terminate. */

  /* If the -X suffix is there delete it.
  */
  if ((p = strrchr(ret_name, SUFFIX[0])) != NULL
  && str_equel(p, SUFFIX))
        *p = '\0';

  if (d_flag && suffix[0] != '\0') {           /* If there is a suffix, */
	p = strchr(ret_name, '.');
	if (p!= NULL)                   /* Allow one suffix char. */
		p[2] = '\0';
	strncat(ret_name, suffix, SUFFIX_LEN); /* Add original suffix. */
  }

#ifdef DOS
  p = ret_name + strlen(ret_name) -1;
  if (*p == '.')
        *p = '\0';   /* Kill empty suffix. */
#endif /* DOS */

  return ret_name;
}

/* new_stin: Set stdin to newin. Return success or fail.
*/
int new_stdin(newin)
char *newin;
{
  if (freopen(newin, "r", stdin) == (FILE *)NULL) {
	fprintf(stderr, "\r%s: Can't open \"%s\"", pname, newin);
	perror(" for reading: ");
	fflush(stderr);
        return 0;               /* Report error. */
  }
  Nin = 0;                      /* Reset input bytecounter. */
  STDIN = newin;

  return 1;
}

/* new_stdout: Set stdout to newout. Return success or fail.
*/
int new_stdout(newout)
char *newout;
{
  FILE *tty;                    /* File pointer for "/dev/tty" */
  static char buf[2];
  char *ans = buf;              /* To contain the answer, "y" or "n" */

  Nout = 0;

  /* If the cat flag is set, don't reopen stdout.
  */
  if (c_flag)
          return 1;             /* Return success. */

  /* Check if we will overwrite a file. (Unless the -f flag was given.)
  */
  if (!f_flag && access (newout, 0) == 0) {
	if (!isatty(2) || (tty = fopen(TTY, "r")) == (FILE *)NULL) {
		fprintf(stderr, "\r%s: Won't overwrite %s\n", pname, newout);
		fflush(stderr);
                return 0;
        }
	fprintf(stderr, "\r%s: Overwrite %s (y/n)? ", pname, newout);
	fflush(stderr);
	ans = fgets(ans, 2, tty);
	if (tty != (FILE *)NULL) fclose(tty);
	if (ans == NULL || *ans != 'y')
		return 0;
  }

  /* Reopen stdout.
  */
  if (freopen(newout, "w", stdout) == (FILE *)NULL) {
	fprintf(stderr, "\r%s: Can't open \"%s\"", pname, newout);
	perror(" for writing: ");
	fflush(stderr);
        return 0;               /* Report fail. */
  }
  STDOUT = newout;
  return 1;                     /* Return success. */
}

/* print_ratio: print the compression ratio if its negative, or the
** verbose flag (v_flag) is TRUE. 
*/
void print_ratio()
{
  long rat;

  if (Nin == 0) {
	if (!v_flag)
		fprintf(stderr, "\r%s: Warning %s ", pname, STDIN);
	fprintf(stderr, "is empty\n");
        return;
  }

  /* Divide Nin and Nout by 2 as long as they are too big, to 
  ** prevent overflowing the rat variable.
  */
  while (Nin > 10000 || Nout > 10000) { /* Get smaller value. */
        Nin >>= 1;
        Nout >>= 1;
  }
  if (Nin == 0) 
	Nin = 1;
  rat = ((Nin - Nout) * 10000) / Nin;

  if (rat < 0L) {
	if (!v_flag)
		fprintf(stderr, "\r%s: ", pname);
	fprintf(stderr, "Warning ");
	if (!v_flag)
		fprintf(stderr, "%s ", STDIN);
  }

  if (rat < 0L || v_flag) {
	fprintf(stderr, "compression: %d", (int)(rat / 100));
	fprintf(stderr, ".%02d%%\n", abs ((int)(rat % 100)));
  }
}

/* Verbose option displays the info structure.
*/
char *info[] = {
  "\rThis is comic Version ", VERSION, "\t  ",
  "(p) Jan-Mark Wams (email: jms@cs.vu.nl)\n",
#ifdef __DATE__
  "Compiled on: ", __DATE__, "\n",
#endif
  "Flags: ",
#ifdef DEBUG
  "DEBUG ",
#endif
#ifdef C89
  "C89 ",
#endif
#ifdef _MINIX
  "MINIX ",
#endif
#ifdef UNIX
  "UNIX ",
#endif
#ifdef AMOEBA
  "AMOEBA ",
#endif
#ifdef M68
  "M68 ",
#endif
#ifdef __BCC__
  "BCC ",
#endif
#ifdef GCC
  "GCC ",
#endif
#ifdef DOS
  "DOS ",
#endif
#ifdef MSC
  "MSC ",
#endif
#ifdef __TURBOC__
  "TURBOC ",
#else
# ifdef __STDC__
#  if ! __STDC__
  "!",
#  endif
  "STDC ",
# endif
#endif
#ifdef NDEBUG
  "NDEBUG ",
#endif
#ifdef _POSIX_SOURCE
  "_POSIX_SOURCE ",
#endif
  "\n",
#ifdef H2_SIZE
  "Hash table: %d,%d",
#else
  "Dynamic hash table",
#endif
  "\n",
  NULL,
};

/* verbose_info: Give verbose information about this program.
*/
void verbose_info()
{
  char **p;
  for (p = info; *p != NULL; p++)
	fprintf(stderr, *p, H1_SIZE, H2_SIZE); /* SIZEs not always needed. */
}

/* Usage: print the usage of comic.
*/
void usage()
{
#ifndef DOS
  char *options = "cdfrsvV?";

  if (str_equel(pname, DECOMIC))
        options = "cfrvV?";
  else if (str_equel(pname, XCAT))
        options = "rvV?";
#else
  char *options = "acdfrSvV?";
  char *optstr;

  if (str_equel(pname, DECOMIC))
        options = "acfrvV?";
  else if (str_equel(pname, XCAT))
        options = "arvV?";
#endif /* DOS */

  fprintf(stderr, "Usage: %s [-%s] [<file>[-X]...]\n", pname, options);

#ifdef DOS              /* DOS has no manual pages so we add extra info. */
  while (*options != '\0') {
        switch (*options) {
          case 'r': optstr = "Raw; don't use a header";            break;
          case 'c': optstr = "Cat; generate ouput on stdout";      break;
          case 'd': optstr = "De-comic; decomic given files";      break;
          case 'f': optstr = "Force; just carry on";               break;
          case 'S': optstr = "Suffix; don't save the suffix";      break;
          case 'a': optstr = "Ascii; %ss ascii files only";        break;
          case 'v': optstr = "Verbose; tell what's going on";      break;
          case 'V': optstr = "Version; give %ss version";          break;
          case '?': optstr = "What; give usage of %s";             break;
          default:  optstr = "even I don't know this one";
        }
	fprintf(stderr, "    -%c:  ", *options);
	fprintf(stderr, optstr, pname); /* Pname not always needed. */
	fprintf(stderr, ".\n");
        options ++;
  }
  fprintf(stderr, "   -ff:  Carry on through fatal errors.\n");
  if (str_equel(pname, COMIC))
	fprintf(stderr, "   -vv:  Extra verbose output.\n");
# ifdef DEBUG
  fprintf(stderr, "  -vvv:  Super verbose (debug) output.\n");
# endif /* DEBUG */
#endif /* DOS */
}


/* File_start: Print a message if verbose option.
*/
void file_start()
{
  if (v_flag) {
	fprintf(stderr, "%s: ", STDIN);
        if (d_flag)
		fprintf(stderr, "decomicing");
  }
  fflush(stderr);
}

/* File_done: Print a message if verbose option.
*/
void file_done()
{
  if (v_flag && d_flag)
	fprintf(stderr, "\b\b\bed to %s\n", STDOUT);
  if (!d_flag)
	print_ratio();
  fflush(stderr);
}

/* 
   For speed's sake, a (two dimensional) hash table is implemented
   on top of the existing structure (circular buffer).
   For each character pair {c1, c2} there exists an index(at hash_tbl 
   [H1(c1), H2(c2)]) containing the index of the first pair {d1, d2}
   in the buffer so that H1(c1) == H1(d1) and H2(c2) == H2(d2). 
   Both Hs are functions ie. if H (c) != H (d) than c != d. The same 
   index is used as an index to the index list, to find the next index. 
   Again this index is used in both the buffer and the index list 
   (next_list) etc. The final index is INDEX_END. There is also an 
   index list pointing back, so the index list can be quickly updated. 
   This leaves us with a double linked list like construction.
*/

indext *hash_tbl = (indext *)NULL;
indext next_list[BUFF_SIZE];                   /* The index list. */
indext back_list[BUFF_SIZE];                   /* The back list. */

  static char *Hash_Mem = NULL;

/* hash_init: Initialise the hash table.
*/
void hash_init()
{
  int i;
#ifndef H2_SIZE
  char _stack_space_needed_by_program[2048];
  _stack_space_needed_by_program[0] |= 1;
#endif

  /* First time Hash_Mem == NULL
  */
  if (Hash_Mem == NULL)
	Hash_Mem = malloc(H1_SIZE * H2_SIZE * sizeof(indext));

#ifndef H2_SIZE
  /* Find the maximum amount of memory, Note that this 
  ** might leave us with a stack space shortage ;-(.
  */ 
  while (Hash_Mem == NULL && H1_SIZE > 4) {
        if (H1_SIZE == H2_SIZE)
	       H2_SIZE >>= 1;
        else 
               H1_SIZE >>= 1;
	Hash_Mem = (char *)malloc(H1_SIZE * H2_SIZE * sizeof(indext));
  }
  if (v_flag > 1)
	fprintf(stderr, "\r%s: Hash size %d,%d\n", pname, H1_SIZE, H2_SIZE);
#endif /* H2_SIZE */

  if (Hash_Mem == NULL) {
	fprintf(stderr, "\r%s: Out of memory.\n", pname);
	exit(-1);
  }
  hash_tbl = (indext *)Hash_Mem;

  /* Init the hash table.
  */
  for (i = 0; i < H1_SIZE * H2_SIZE; i++)
	hash_tbl[i] = INDEX_END;

  /* Fill the next_list and back_list.
  */
  next_list[0] = BUFF_SIZE - 1;
  back_list[BUFF_SIZE - 1] = 0;
  for (i = 1; i < BUFF_SIZE; i++) {
	next_list[i] = i - 1;
	back_list[i - 1] = i;
  }

  /* Update the hash table for a buffer full of zeros.
  */
  hash_entry('\0', '\0') = i = Bindex(Bsub(Bp, 2));
  back_list[i] = INDEX_END;
  next_list[Bindex(Bsub(Bp, OFFSET_MAX))] = INDEX_END;
}


/* Hash_update: Update the hash table. (Using the char at Bp)
*/
void hash_update()
{
  char p = *Bpred(Bp);                         /* Previous char. */
  char c = *Bp;                                 /* Current char. */
  indext icur = Bindex(Bp);                    /* Current index. */
  indext ipre = Ipred(icur);                   /* Previous index. */
  char *  tail = Bsub(Bp, OFFSET_MAX);         /* Tail of offset buff. */
  indext new_tail = back_list[Bindex(tail)];  /* New tail of next list. */
  indext ipc = hash_entry(p, c);               /* Index of hash (p,c). */

  if (next_list[Bindex(tail)] != INDEX_END)
        *Bp = *Bp;

  /* assert (next_list[Bindex(tail)] == INDEX_END); @@@ */

  if ((next_list[ipre] = ipc) != INDEX_END)
  back_list[ipc] = ipre;
  hash_entry(p, c) = ipre;             /* Update list at head of buffer. */
  back_list[ipre] = INDEX_END;

  if (new_tail == INDEX_END) {          /* Idem tail. */
	hash_entry(*tail, *Bsucc (tail)) = INDEX_END;
  }
  else {
	next_list[new_tail] = INDEX_END;
  }
}

/*
   Decoding: Note we always decode stdin to stdout. Get a bit, 
   if it's a 0, decode a pair, if it's a 1, decode a char. If 
   it's 1 nor 0, fly the broomstick. *8-)
*/

/* decode_pair: Read offset and length, and put substring on stdout.
*/
void decode_pair()
{
  int len, off = Hdecode(Hoff);
  char *p;

  off = off << LOW_OFFSET_BITS;
  off |= get_n_bits(LOW_OFFSET_BITS);
  off += OFFSET_MIN;
  len = Hdecode(Hlen) + LENGTH_MIN;
#ifdef DEBUG
  if (v_flag > 1)
	printf("<%d,%d>\"", off, len);
#endif
  p = Bsub(Bp, off);
  while (len --) {
        putchar (*p);
        *Bp = *p;
	p = Bsucc(p);
	Bp = Bsucc(Bp);
  }
#ifdef DEBUG
  if (v_flag > 1)
# ifdef DOS
	printf("\"\r\n");
# else
	printf("\"\n");
# endif
#endif
}

/* decode_char: Get a huffman encoded char and put it on stdout.
*/
void decode_char()
{
  int c = Hdecode(Hchar);
  putchar (c);
#ifdef DEBUG
  if (v_flag > 1) {
# ifdef DOS
        putchar ('\r');
# endif
        putchar ('\n');
  }
#endif
  *Bp = c;
  Bp = Bsucc(Bp);
}

/* get_input: Get input buffer full, return EOF if end of file, SPC otherwise.
*/
int get_input()
{
  static unsigned long fsize = 0L;
  static struct stat st[1];
  static int printed = -1;              /* -1 Means don't print. */
  int percent, c;
  char *stop = Blookupstart(), *Oinputend = Binputend;

  if (v_flag > 1 && Nin == 0 && fstat (0, st) == 0 
      && S_ISREG (st[0].st_mode)) {
	fsize = (unsigned long) (st[0].st_size);
	fprintf(stderr, "  0%%");
	fflush(stderr);
        printed = 0;
  }

/*
   Read stdin till stop. This code may look complicated, but it
   isn't. Most of it is about printing.
*/

  while (Binputend != stop && (c = getchar()) != EOF) {
        *Binputend = c;
#ifdef DOS
        if (a_flag && c == '\n')        /* Size of file is including \r. */
                Nin += 1;               /* But we don't read it, so adjust. */
#endif
	Binputend = Bsucc(Binputend);  /* Increase the input end marker. */
  }
  Nin += Bdelta(Binputend, Oinputend); /* Count the number of char's read. */

  if (Nin < fsize) {    /* Note 0 <=  Nin < fsize A thus 0 < fsize */
        percent = 100 - (int)((fsize - Nin) / (fsize / 100));
        if (percent > printed) {
		fprintf(stderr, "\b\b\b\b%3d%%", percent);
		fflush(stderr);
                printed = percent;
        }
  }

  if (Bp == Binputend) {
        assert (Oinputend == Binputend);
#ifdef DOS
        if (a_flag && fsize != 0 && Nin != fsize && Nin != fsize -1) {
		fprintf(stderr, "\r%s: -a used on binary data:", pname);
		fprintf(stderr, " aberrant decomic results\n");
        }
        else
                assert (fsize == 0 || Nin == fsize || Nin == fsize - 1);
#else
        /* This assertion might fail if one comics a binary file under MS-DOS.
        */
        assert (fsize == 0 || Nin == fsize);
#endif /* DOS */
        if (printed != -1) {
		fprintf(stderr, "\b\b\b\b");
		fflush(stderr);
        }
        return EOF;
  }
  else
	return (int)' ';
}

/*
   To find the best (longest) part to output we try to find the longest 
   part matching the input in the lookup buffer. Well actually, we look 
   for two srings, one, matching the current input, starting at the current 
   buffer start, and one matching the string starting at the next input 
   byte, (as if we decoded a char.) This is just in case decoding two 
   pairs could have been done, using one char and pair. (This will save 
   a few % extra.) To show what I mean, consider:
  
                                        xxxxx.AxAxxxxx
        This would be parsed            x xxxx . A x Ax xxxx
        but xxxx costs (about) as
        much as xxxxx. So               x xxxx . A x A xxxxx
        will be better.
*/


/* Eqlen: Return the number of bytes for which p[i] and q[i] are the same.
*/
int eqlen(p, q, count)
char *p;
register char *q;
int count;
{
  register char *r;
  char *stop, save;

  assert (p != q);

#ifdef OLD_SLOW_CODE
  /*
      If the code below look fussy, it's just a quick version of:
  */
  while (*r == *q && r != stop) {
	r = Bsucc(r);
	q = Bsucc(q);
  }
#else

  if (p > q) {
	r = q;
	q = p;
	p = r;
  }
  else
	r = p;

  /*
      +-------------------------+-+
      |           Buff          | | 
      +-------------------------+-+
      p=r^   q^   stop^    Bend^ ^Sentp
  */

  assert (r < q);

  /* Prevent q from equeling stop in while loop. */
  if (q - r <= count) {
	stop = Badd(q, count);
	save = *stop;
	*stop = ~*Badd(r, count);
  }
  else {
	stop = Badd(r, count);
	save = *stop;
	*stop = ~*Badd(q, count);
  }

  *Sentp = ~r[Sentp - q];
  while (*r == *q) {
	r++; 
	q++;
  }
  if (q == Sentp) {
	q = Buff;
	*Sentp = ~Buff[Sentp - r];
	while (*r == *q) {
		r++;
		q++;
	}
	if (r == Sentp) {
		r = Buff;
		while (*r == *q) {
			r++;
			q++;
		}
	}
  }
  *stop = save;
#endif 

  assert (Bdelta(r, p) <= count);
  return Bdelta(r, p);
}


/* get_token: Find the best (longest) part to output.
*/
void get_token(pair_0used)
int pair_0used;             /* Indicate if pair_0 was used. */
{
  register indext i;
  register int len;             /* Used to store the result of eqlen. */
  register int length;          /* Used for p[01]->length. */
  register char *start = NULL;  /* Used for p[01]->start. */

  /* Itail is a valid start value for pair_0, but not for pair_1.
  */
  indext itail = Bindex(Bsub(Bp, OFFSET_MAX)); /* Last index. */

  int maxlen = Bdelta(Binputend, Bp);  /* Max match length. */
  char *Bp1 = Bsucc(Bp);            /* Successor of Bp. */
  char *Bp2 = Bsucc(Bp1);           /* Guess... */
  char *Bp_1 = Bpred(Bp);

  if (maxlen < LENGTH_MIN) {
	pair_0->length = 0;
        return;
  }

  if (maxlen > LENGTH_MAX) {
        assert (maxlen <= LENGTH_MAX + PAIR_MAX + 1);
        maxlen = LENGTH_MAX;
  }

  if (!pair_0used) {                      /* If pair_0 wasn't used, pair_1 */
	pair_0->start = pair_1->start;   /* will be the new pair_0. */
	     pair_0->length = pair_1->length; /* pair_1->length might be */
	     if (pair_0->length >= maxlen) {   /* one too big. */
			  pair_0->length = maxlen;
			  pair_1->length = 0;
		          return;                    /* We can't beat maxlen. */
        }
  }
  else {
        if (*Bp_1 == *Bp && *Bp == *Bp1) {    /* Bp - 1 isn't in the hash */
                 start = Bp_1;
		 length = eqlen(Bp_1, Bp, maxlen);
                 if (length == maxlen) {
                         pair_0->start = start;   /* This test saves time */
                         pair_0->length = length; /* while comic-ing binary */
                         pair_1->length = 0;      /* data (eg. tar files). */
                         return;
                 }
        }         
        else
                length = 0;

	i = hash_entry(*Bp, *Bp1);

        while (i != INDEX_END) {
		if (*Badd(Buff + i, length) == *Badd(Bp, length)
		&& (len = eqlen(Buff + i, Bp, maxlen)) > length) {
                        start = Buff + i;
                        length = len;
                }
		i = next_list[i];
        }
	pair_0->start = start;
	pair_0->length = length;
  }

  /* We should adjust maxlen here, but we get faster code if we just check 
  ** if pair_1->length is (one) too long whilst copying pair_1 to pair_0.
  */

  if (*Bp == *Bp1 && *Bp1 == *Bp2) {   /* Bp1 - 1 isn't in the hash. */
        start = Bp;
	length = eqlen(Bp, Bp1, maxlen);
        if (length == maxlen) {
		pair_1->start = start;
		pair_1->length = length;
                return;
        }
  }
  else if (*Bp_1 == *Bp1) {             /* Bp1 - 2 neither. */
        start = Bp_1;
	length = eqlen(Bp_1, Bp1, maxlen);
        if (length == maxlen) {
		pair_1->start = start;
		pair_1->length = length;
                return;
        }
  }
  else
        length = 0;

  i = hash_entry(*Bp1, *Bp2);          /* Do pair_1. */

  while (i != INDEX_END /* (i != itail) is done in if below. */) {
	if (*Bp != Buff[Ipred(i)])
	if (*Badd(Buff + i, length) == *Badd(Bp1, length))
	if ((len = eqlen(Buff + i, Bp1, maxlen)) > length)
	if (i != itail) {
		start = Buff + i;
		length = len;
	}
	i = next_list[i];
  }

  pair_1->start = start;
  pair_1->length = length;

  assert (pair_0->length <= maxlen);
  assert (pair_1->length <= maxlen);
}

/* Magic_ok: Check the first bytes on stdin. They should be ok.
** Return True if magic is ok, else False. Also set suffix if one
** is there.
*/
int magic_ok()
{
  char c1, c2;

  if (r_flag)
        return 1; /* No header in raw mode. */

#ifdef DOS
  setmode(0, O_BINARY);
#endif

  if (read (0, &c1, 1) != 1) {
	fprintf(stderr, "\r%s: Can't read header\n", pname);
        if (f_flag < 2) return 0;
  }

  if (read (0, &c2, 1) != 1) {
	fprintf(stderr, "\r%s: Can't read header's 2nd byte\n", pname);
        if (f_flag < 2) return 0;
  }

  if (c1 != MAGIC1 || (c2 & 0xF0) != MAGIC2) {
	fprintf(stderr, "\r%s: Wrong magic in header\n", pname);
        if (f_flag < 2) return 0;
  }

  suffix[0] = '\0';
  if ((c2 & SUFFIX_BIT) != 0 && read (0, suffix, SUFFIX_LEN) != SUFFIX_LEN) {
	fprintf(stderr, "\r%s: Can't read suffix bytes\n", pname);
        if (f_flag < 2) return 0;
  }

  if ((c2 & DYNAMIC_BIT) != 0 || (c2 & STATIC_BIT) == 0) {
	fprintf(stderr, "\r%s: Version %s ", pname, VERSION);
	fprintf(stderr, "can't handle dynamic bit\n");
        if (f_flag < 2) return 0;
  }
  if ((c2 & EXTEND_BIT) != 0) {
	fprintf(stderr, "\r%s: Version %s ", pname, VERSION);
	fprintf(stderr, "can't handle extend bit\n");
        if (f_flag < 2) return 0;
  }

  return 1;
}

/* Put_magic: Write magic header on stdout. Append MS-DOS suffix to.
*/
void put_magic()
{
  char c1 = MAGIC1, c2 = (MAGIC2 | STATIC_BIT);

  if (r_flag)
        return; /* Don't put magic in raw mode. */

  /* Put the suffix bits if necessary.
  */
  if (s_flag && suffix[0] != '\0')
      c2 |= SUFFIX_BIT;

  if (write (1, &c1, 1) != 1 || write (1, &c2, 1) != 1) {
	fprintf(stderr, "\r%s: Can't write header\n", pname);
	exit(-1);
  }
#ifdef DOS
  setmode(1, O_BINARY);
#endif
  if (s_flag && suffix[0] != '\0') {
        if (write (1, suffix, SUFFIX_LEN) != SUFFIX_LEN) {
		fprintf(stderr, "\r%s: Can't write suffix\n", pname);
		exit(-1);
        }
        Nout += SUFFIX_LEN;  /* Account two byte suffix. */
  }
  Nout += 2;    /* Account two bytes magic. */
}

/* 
   H (huffman) routines to get and put huffman codes. Note: If the 
   Hencode is called for SWITCH codes with a codelength > HUFFMAN_BITS,
   the actual code is used. This is mainly an optimisation for binary 
   chunks in the text. This could be replaced by dynamic huffman code.
   (See f.e. "Data Compression" ACM Computing Surveys, Vol.19 No. 3 
   September 1987). I tried modified BSTW, but that wasn't too good.
*/

#define SWITCH 8

#if OFFSET_BITS != 13
# include "COMILER ERROR: the tables are generated for 13 bits."
#endif

struct ctab_s * ctab_tab[] = { ctab_char, ctab_off, ctab_len };
struct tree_s * tree_tab[] = { tree_char, tree_off, tree_len };
int Ntolong[3];                /* Ntolong for each table. */

/* Descend: descend down the tree.
*/
int descend (tree)
struct tree_s *tree;
{
  int i = (HUFFMAN_SIZE * 2) - 2;
  int bit;

  while (i >= HUFFMAN_SIZE) {
        bit = get_bit();
        if (bit == 0) {
                i = tree[i - HUFFMAN_SIZE]._0;
        } else
        if (bit == 1) {
                i = tree[i - HUFFMAN_SIZE]._1;
        } else
        if (bit == EOF) {
                fprintf(stderr, "\r%s: Unexpected EOF\n", pname);
                exit(-1);
        } else {
                fprintf(stderr, "\r%s: Funny bit\n", pname);
                exit(-1);
        }
  }
  return i;
}


/* Hinit: Get the proper huffman code tables.
*/
void Hinit()
{
  Ntolong[0] = 0;
  Ntolong[1] = 0;
  Ntolong[2] = 0;
}


/* Hdecode: Get an int using a huffman tree.
*/
int Hdecode(index)
int index;
{
  struct tree_s *tree = tree_tab[index];
  struct ctab_s *ctab = ctab_tab[index];
  int i;

  if (Ntolong[index] > SWITCH) {
	i = (int)get_n_bits(HUFFMAN_BITS);
	if (ctab[i].length > HUFFMAN_BITS)
		Ntolong[index] = SWITCH << 1;
        else
		Ntolong[index] -= 1;
  }
  else {
        i = descend (tree);
	if (ctab[i].length > HUFFMAN_BITS)
		Ntolong[index] += 1;
        else
		Ntolong[index] = 0;
  }
  return i;
}


/* Hcodelen: Return the length of the encoded word in bits.
*/
int Hcodelen(index, c)
int index;
int c;
{
  return Ntolong[index] > SWITCH ? HUFFMAN_BITS : ctab_tab[index][c].length;
}

/* Hencode: output code i from code table ctab
*/
void Hencode(index, i)
int index, i;
{
  struct ctab_s *ctab = ctab_tab[index];
  int len = ctab[i].length;

  if (Ntolong[index] > SWITCH) {
	put_n_bits((long)i, HUFFMAN_BITS);
        if (len > HUFFMAN_BITS)
		Ntolong[index] = SWITCH << 1;
        else
		Ntolong[index] -= 1;
  }
  else {
	put_n_bits((long)(ctab[i].code), len);
        if (len > HUFFMAN_BITS)
		Ntolong[index] += 1;
        else
		Ntolong[index] = 0;
  }
}


/* Decode: decode stdin. Read a bit from stdin, if it's a 0, decode a pair,
** if it's a 1 decode a char, if it's EOF, return.
*/
void decode()
{
  int bit;

  Bsize = OFFSET_SIZE + OFFSET_MIN;
  Bend = &(Buff[OFFSET_MAX]);
  Bp = Binputend = Buff;        /* Not necessary, but.. */
  Binit();                     /* Init the buffer. */
  Hinit();                     /* Init the huffman tables. */
  init_bits();                 /* Init the bit package. */

#ifdef DOS
  setmode(0, O_BINARY);        /* And you wonder why they call it SM_DOS. */
  if (a_flag)
	setmode(1, O_TEXT);
  else
	setmode(1, O_BINARY);
#endif

  for (;;) {
        bit = get_bit();
        if (bit == 0) {
                decode_pair();
        } else
        if (bit == 1) {
                decode_char();
        } else
        if (bit == EOF) {
                fflush(stdout);
                return;                 /* EOF: we're done */
        } else {
                fprintf(stderr, "\r%s: Funny bit\n", pname);
                exit(-1);
        }
  }
}


/*
   Encoding is simple: Get next two longest strings, if the second 
   one is longer, or the first one isn't long enough, encode a char,
   else encode a pair, (offset, length).
*/

/* output_pair: Output pair_0's length and offset.
*/
void output_pair(done)
int *done;      /* Set to False if pair codes are longer than char codes. */
{
  int offset = Bdelta(Bp, pair_0->start) - OFFSET_MIN;
  int length = pair_0->length;

  assert (offset >= 0);
  assert (offset < OFFSET_SIZE);
  assert (length >= LENGTH_MIN);
  assert (length - LENGTH_MIN < HUFFMAN_SIZE);

#if LENGTH_MIN <= 2
  if (length == 2
  &&
      Hcodelen(Hoff, offset >> LOW_OFFSET_BITS) + LOW_OFFSET_BITS
    + Hcodelen(Hlen, 2 - LENGTH_MIN)
    >
      Hcodelen(Hchar, 0xFF & (int)*Bp) 
    + Hcodelen(Hchar, 0xFF & (int)*Bsucc(Bp))) {
	*done = 0;
	return;
  }
#endif

  put_bit(0);                                  /* Put a 0. */
  Hencode(Hoff, offset >> LOW_OFFSET_BITS);    /* Put offset high. */
  put_n_bits((long)offset, LOW_OFFSET_BITS);   /* Put offset low. */
  Hencode(Hlen, length - LENGTH_MIN);          /* Put index length. */

#ifdef DEBUG
  if (v_flag > 2) {
	fprintf(stderr, "(%d,%d)", offset + OFFSET_MIN, length);
	putc('"', stderr);
  }
#endif
        while (length -- > 0) {
#ifdef DEBUG
		if (v_flag > 2) putc(*Bp, stderr);
#endif
		hash_update();         /* Update hash tables. */
		Bp = Bsucc(Bp);
        }
#ifdef DEBUG
        if (v_flag > 2) {
		fprintf(stderr, "\"\n");
		fflush(stderr);
        }
#endif /* DEBUG */
  assert (done);
}

/* output_char: Output char *Bp.
*/
void output_char()
{
  put_bit(1);                          /* Put a 1. */
  Hencode(Hchar, (0xFF & (int)*Bp));   /* Put this char. */
#ifdef DEBUG
  if (v_flag > 2) {
	putc('`', stderr);
	putc(*Bp, stderr);
	putc('\'', stderr);
	putc('\n', stderr);
  }
#endif /* DEBUG */
  hash_update();
  Bp = Bsucc(Bp);                      /* Increase Bp. */
}


/* Encode: parse stdin, write huffman codes and bits on stdout.
*/
void encode()
{
  /* Pairout indicates the pair pair_0 will be output, not just char *Bp.
  */
  int pairout = 1;
  Bsize = BUFF_SIZE;
  Bend = &(Buff[BUFF_MAX]);
  Bp = Binputend = Buff;        /* Not necessary, but.. */
  Binit();                     /* Init the buffer. */
  Hinit();                     /* Init the huffman tables. */
  init_bits();                 /* Init the bit package. */
  hash_init();                 /* Init the hash tables. */

#ifdef DOS
  if (a_flag)
	setmode(0, O_TEXT);
  else
	setmode(0, O_BINARY);  /* And you wonder why they call it SM_DOS. */

  setmode(1, O_BINARY);
#endif

  while (get_input() != EOF) {
	get_token(pairout);                      /* Find matching strings. */
	pairout =  pair_0->length >= LENGTH_MIN  /* Long enough? */
		&& pair_0->length >= pair_1->length;
        if (pairout)
		output_pair(&pairout);           /* If so, output its pair. */
	if (!pairout)
		output_char();                   /* Else do one char. */
  }
  flush_bits();                                  /* Flush buffered bits. */
}

/* main: Parse arguments, call proper (en/de) code routine.
*/
int main(argc, argv)
int argc;
char **argv;
{
  char *p;              /* Used for flag processing etc. */
  char *fname;

#ifdef DOS
  /* DOS-file names are in UPPER case.
  */
  str_2_upper(SUFFIX);
#endif

  pname = basename(*argv);

  argc --;
  argv ++;      /* Skip pname. */


  if (str_equel(pname, DECOMIC))   /* If this program is called */
        d_flag = -1;            /* decomic set the decode flag. */

  if (str_equel(pname, XCAT)) {    /* If this program is called */
        d_flag = -1;            /* xcat set the decode */
        c_flag = -1;            /* and cat flag. */
  }

  while (argc > 0 && (*argv)[0] == '-' && (*argv)[1] != '\0') {
        for (p = *argv + 1; *p != '\0'; p++) {
                switch (*p) {
                case 'd': d_flag = 1; break;           /* decode flag on. */
                case 'r': r_flag = 1; break;           /* Raw flag. */
                case 'v': v_flag ++; break;            /* Verbose flag. */
                case 'f': f_flag ++; break;            /* Force flag on. */
#ifdef DOS
                case 'a': a_flag = 1; break;           /* Ascii mode. */
                case 'S': s_flag = 0; break;           /* No suffix save. */
#else
                case 's': s_flag = 1; break;           /* Save suffix. */
#endif
                case 'c': c_flag = 1; break;           /* Cat flag on. */
		case 'V': verbose_info(); exit(0);   /* Verbose info. */
		case '?': usage(); exit(0);          /* Usage and exit. */
                default :
		    fprintf(stderr, "\r%s: -%c ignored\n", pname, *p);
		    fflush(stderr);
                }
        }
        argc --; argv ++;	/* Next please. */
  }

  if (argc == 0) {		/* No args, do stdin to stdout. */
        c_flag = 1;		/* Cat mode. */
	file_start();
        if (d_flag) {
		if (magic_ok())
			decode();
        }
        else {
		put_magic();
		encode();
        }
	file_done();
  }
  else {
        if (c_flag && !d_flag && f_flag < 2 && argc > 1) {
		fprintf(stderr, "\r%s: Won't concatanate ", pname);
		fprintf(stderr, "multiple files.\n");
		exit(-1);
        }
  }

  while ((fname = next_arg()) != NULL) {
#ifdef DOS
	str_2_upper(fname);
#endif
        if (d_flag) {   /* Decode. */
		if ( !new_stdin(new_name(fname))
		||   !magic_ok()
		||   !new_stdout(org_name(fname)))
                        continue;       /* If not reopened, do next file. */
		file_start();
		decode();      /* Decode stdin. */
		file_done();
        }
        else /* No d_flag */ {  /* Encode. */
		if ((p = strrchr(fname, *SUFFIX)) != NULL
		&&  strncmp(p, SUFFIX, SUFFIX_LEN) == 0) {
                        if (v_flag) {
				fprintf(stderr, "\r%s: ", pname);
				fprintf(stderr, "%s already has ", fname);
				fprintf(stderr, "%s suffix\n", SUFFIX);
				fflush(stderr);
                        }
                        continue;
                }
		if ( !new_stdin(org_name(fname))
		||   !new_stdout(new_name(fname)))
                        continue;       /* If not reopened, do next file. */
		file_start();
		put_magic();
		encode();              /* Put data on stdout. */
		file_done();
        }
  }
  return 0;
}
