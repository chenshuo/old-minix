/* sort - sort a file of lines		Author: Michiel Huisjes */

/* SYNOPSIS:
 * 	sort [-funbirdcmt'x'] [+beg_pos[opts] [-end_pos]] [-o outfile] [file] ..
 * 
 * 	[opts] can be any of
 * 	-f : Fold upper case to lower.
 * 	-n : Sort to numeric value (optional decimal point) implies -b
 * 	-b : Skip leading blanks
 * 	-i : Ignore chars outside ASCII range (040 - 0176)
 * 	-r : Reverse the sense of comparisons.
 * 	-d : Sort to dictionary order. Only letters, digits, comma's and points
 * 	     are compared.
 * 	If any of these flags are used in [opts], then they override all global
 * 	ordering for this field.
 * 
 * 	I/O control flags are:
 * 	-u : Print uniq lines only once.
 * 	-c : Check if files are sorted in order.
 * 	-m : Merge already sorted files.
 * 	-o outfile : Name of output file. (Can be one of the input files).
 * 		     Default is stdout.
 * 	- : Take stdin as input.
 * 
 * 	Fields:
 * 	-t'x' : Field separating character is 'x'
 * 	+a.b : Start comparing at field 'a' with offset 'b'. A missing 'b' is
 * 	       taken to be 0.
 * 	-a.b : Stop comparing at field 'a' with offset 'b'. A missing 'b' is
 * 	       taken to be 0.
 * 	A missing -a.b means the rest of the line.
 */

#include "stat.h"
#include "signal.h"

#define OPEN_FILES	16		/* Nr of open files per process */

#define MEMORY_SIZE	(20 * 1024)	/* Total mem_size */
#define LINE_SIZE	(1024 >> 1)	/* Max length of a line */
#define IO_SIZE		(2 * 1024)	/* Size of buffered output */
#define STD_OUT		 1		/* Fd of terminal */

/* Return status of functions */
#define OK		 0
#define ERROR		-1
#define NIL_PTR		((char *) 0)

/* Compare return values */
#define LOWER		-1
#define SAME		 0
#define HIGHER		 1

/*
 * Table definitions.
 */
#define DICT		0x001		/* Alpha, numeric, letters and . */
#define ASCII		0x002		/* All between ' ' and '~' */
#define BLANK		0x004		/* ' ' and '\t' */
#define DIGIT		0x008		/* 0-9 */
#define UPPER		0x010		/* A-Z */

typedef enum {				/* Boolean types */
  FALSE = 0,
  TRUE
} BOOL;

typedef struct {
  int fd;				/* Fd of file */
  char *buffer;				/* Buffer for reads */
  int read_chars;			/* Nr of chars actually read in buffer*/
  int cnt;				/* Nr of chars taken out of buffer */
  char *line;				/* Contains line currently used */
} MERGE	;

#define NIL_MERGE	((MERGE *) 0)
MERGE merge_f[OPEN_FILES];		/* Merge structs */
int buf_size;			/* Size of core available for each struct */

#define FIELDS_LIMIT	10		/* 1 global + 9 user */
#define GLOBAL		 0

typedef struct {
  int beg_field, beg_pos;		/* Begin field + offset */
  int end_field, end_pos;		/* End field + offset. ERROR == EOLN */
  BOOL reverse;				/* TRUE if rev. flag set on this field*/
  BOOL blanks;
  BOOL dictionary;
  BOOL fold_case;
  BOOL ascii;
  BOOL numeric;
} FIELD;

/* Field declarations. A total of FILEDS_LIMIT is allowed */
FIELD fields[FIELDS_LIMIT];
int field_cnt;			/* Nr of field actually assigned */

/* Various output control flags */
BOOL check = FALSE;
BOOL only_merge = FALSE;
BOOL uniq = FALSE;

char *mem_top;			/* Mem_top points to lowest pos of memory. */
char *cur_pos;			/* First free position in mem */
char **line_table;		/* Pointer to the internal line table */
BOOL in_core = TRUE;		/* Set if input cannot all be sorted in core */

			/* Place where temp_files should be made */
char temp_files[] = "/tmp/sort.XXXXX.XX";
char *output_file;		/* Name of output file */
int out_fd;			/* Fd to output file (could be STD_OUT) */
char out_buffer[IO_SIZE];	/* For buffered output */

char **argptr;			/* Pointer to argv structure */
int args_offset;		/* Nr of args spilled on options */
int args_limit;			/* Nr of args given */

char separator;			/* Char that separates fields */
int nr_of_files = 0;		/* Nr_of_files to be merged */
int disabled;			/* Nr of files done */

char USAGE[] = "Usage: sort [-funbirdcmt'x'] [+beg_pos [-end_pos]] [-o outfile] [file] ..";

/* Forward declarations */
char *file_name (), *skip_fields ();
MERGE *skip_lines (), *print ();
extern char *msbrk (), *mbrk ();

/*
 * Table of all chars. 0 means no special meaning.
 */
char table[256] = {
/* '^@' to space */
  0,	0,		0,	0,	0,	0,	0,	0,
  0,	BLANK | DICT,	0,	0,	0,	0,	0,	0,
  0,	0,		0,	0,	0,	0,	0,	0,
  0,	0,		0,	0,	0,	0,	0,	0,

/* space to '0' */
  BLANK | DICT | ASCII,	ASCII,	ASCII,	ASCII,	ASCII,	ASCII,	ASCII,
  ASCII,			ASCII,	ASCII,	ASCII,	ASCII,	ASCII,	ASCII,
  ASCII,			ASCII,

/* '0' until '9' */
  DIGIT | DICT | ASCII,	DIGIT | DICT | ASCII,	DIGIT | DICT | ASCII,
  DIGIT | DICT | ASCII,	DIGIT | DICT | ASCII,	DIGIT | DICT | ASCII,
  DIGIT | DICT | ASCII,	DIGIT | DICT | ASCII,	DIGIT | DICT | ASCII,
  DIGIT | DICT | ASCII,

/* ASCII from ':' to '@' */
  ASCII,	ASCII,	ASCII,	ASCII,	ASCII,	ASCII,	ASCII,

/* Upper case letters 'A' to 'Z' */
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,	UPPER | DICT | ASCII,
  UPPER | DICT | ASCII,	UPPER | DICT | ASCII,

/* ASCII from '[' to '`' */
  ASCII,	ASCII,	ASCII,	ASCII,	ASCII,	ASCII,

/* Lower case letters from 'a' to 'z' */
  DICT | ASCII,	DICT | ASCII,	DICT | ASCII,	DICT | ASCII,
  DICT | ASCII,	DICT | ASCII,	DICT | ASCII,	DICT | ASCII,
  DICT | ASCII,	DICT | ASCII,	DICT | ASCII,	DICT | ASCII,
  DICT | ASCII,	DICT | ASCII,	DICT | ASCII,	DICT | ASCII,
  DICT | ASCII,	DICT | ASCII,	DICT | ASCII,	DICT | ASCII,
  DICT | ASCII,	DICT | ASCII,	DICT | ASCII,	DICT | ASCII,
  DICT | ASCII,	DICT | ASCII,

/* ASCII from '{' to '~' */
  ASCII,	ASCII,	ASCII,	ASCII,

/* Stuff from -1 to -177 */
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0,	0,	0,	0,	0,
  0,	0,	0,	0,	0