/*
 * Bawk constants and variable declarations.
 */
#ifdef MAIN
#    define EXTERN
#else
#    define EXTERN extern
#endif

#include <minix/config.h>
#if (CHIP == M68000)
#    define INT		long
#    define ALIGN(p)		(((long)(p) & 1) ? ++(p) : (p) )
#else
#    define INT		int
#    define ALIGN(p)		(p)
#endif

/*#define DEBUG 1  remove this line to compile without debug statements */
#ifdef DEBUG
EXTERN char Debug;              /* debug print flag */
#endif

/*
 * Table and buffer sizes
 */
#define MAXLINELEN      128     /* longest input line */
#define MAXWORDS        (MAXLINELEN/2)  /* max # of words in a line */
#define MAXWORKBUFLEN   4096    /* longest action or regular expression */
#define MAXVARTABSZ     50      /* max # of symbols */
#define MAXVARLEN       10      /* symbol name length */
#define MAXSTACKSZ      40      /* max value stack length (for expressions) */


/**********************************************************
 * Current Input File variables                           *
 **********************************************************/
/*
 * Current Input File pointer:
 */
EXTERN FILE *Fileptr;
EXTERN char *Filename;          /* current input file name */
EXTERN char *Filechar;		/* ptr to next input char if input is string */
EXTERN int Linecount;           /* current input line number */
EXTERN int Recordcount;         /* record count */
/*
 * Working buffers.
 */
EXTERN char Linebuf[ MAXLINELEN ];      /* current input line buffer */
EXTERN char *Fields[ MAXWORDS ];        /* pointers to the words in Linebuf */
EXTERN int Fieldcount;                  /* and the # of words */
EXTERN char Workbuf[ MAXWORKBUFLEN ];   /* work area for C action and */
                                        /* regular expression parsers */

/**********************************************************
 * Regular Expression Parser variables                    *
 **********************************************************/
/*
 * Tokens:
 */
#define CHAR    1
#define BOL     2
#define EOL     3
#define ANY     4
#define CLASS   5
#define NCLASS  6
#define STAR    7
#define PLUS    8
#define MINUS   9
#define ALPHA   10
#define DIGIT   11
#define NALPHA  12
#define PUNCT   13
#define RANGE   14
#define ENDPAT  15


/**********************************************************
 * C Actions Interpreter variables                        *
 **********************************************************/
/*
 * Tokens:
 */
#define T_STRING        'S'     /* primaries: */
#define T_DOLLAR        '$'
#define T_REGEXP        'r'
#define T_CONSTANT      'C'
#define T_VARIABLE      'V'
#define T_FUNCTION      'F'
#define T_SEMICOLON     ';'     /* punctuation */
#define T_EOF           'Z'
#define T_LBRACE        '{'
#define T_RBRACE        '}'
#define T_LPAREN        '('
#define T_RPAREN        ')'
#define T_LBRACKET      '['
#define T_RBRACKET      ']'
#define T_COMMA         ','
#define T_ASSIGN        '='     /* operators: */
#define T_MUL           '*'
#define T_DIV           '/'
#define T_MOD           '%'
#define T_ADD           '+'
#define T_SUB           '-'
#define T_SHL           'L'
#define T_SHR           'R'
#define T_LT            '<'
#define T_LE            'l'
#define T_GT            '>'
#define T_GE            'g'
#define T_EQ            'q'
#define T_NE            'n'
#define T_NOT           '~'
#define T_AND           '&'
#define T_XOR           '^'
#define T_IOR           '|'
#define T_LNOT          '!'
#define T_LAND          'a'
#define T_LIOR          'o'
#define T_INCR          'p'
#define T_DECR          'm'
#define T_IF            'i'     /* keywords: */
#define T_ELSE          'e'
#define T_WHILE         'w'
#define T_BREAK         'b'
#define T_CHAR          'c'
#define T_INT           't'
#define T_BEGIN         'B'
#define T_END           'E'
#define T_NF            'f'
#define T_NR            '#'
#define T_RS            '\n'
#define T_FILENAME      'z'

#define PATTERN 'P'     /* indicates C statement is within a pattern */
#define ACTION  'A'     /* indicates C statement is within an action */

/*
 * Symbol Table values
 */
#define ACTUAL          0
#define LVALUE          1
#define BYTE            1
#define WORD            sizeof(INT)	/* ugh ! */
/*
 * Symbol table
 */
struct variable {
        char    vname[ MAXVARLEN ];
        char    vclass;
        char    vsize;
        int     vlen;
        char    *vptr;
};
#define VARIABLE struct variable
EXTERN VARIABLE Vartab[ MAXVARTABSZ ], *Nextvar;
/*
 * Value stack
 */
union datum {
        INT     ival;
        char    *dptr;
        char    **ptrptr;
};
#define DATUM union datum
struct item {
        char    class;
        char    lvalue;
        char    size;
        DATUM   value;
};
#define ITEM struct item
EXTERN ITEM Stackbtm[ MAXSTACKSZ ], *Stackptr, *Stacktop;
/*
 * Miscellaneous
 */
EXTERN char *Actptr;    /* pointer into Workbuf during compilation */
EXTERN char Token;      /* current input token */
EXTERN DATUM Value;     /* and its value */
EXTERN char Saw_break;  /* set when break stmt seen */
EXTERN char Where;      /* indicates whether C stmt is a PATTERN or ACTION */
EXTERN char Recordsep[20];       /* record seperator */
EXTERN char *Beginact;  /* BEGINning of input actions */
EXTERN char *Endact;    /* END of input actions */

/**********************************************************
 * Rules structure                                        *
 **********************************************************/
struct rule {
        struct {
                char *start;    /* C statements that match pattern start */
                char *stop;     /* C statements that match pattern end */
                char startseen; /* set if both a start and stop pattern */
                                /* given and if an input line matched the */
                                /* start pattern */
        } pattern;
        char *action;           /* contains quasi-C statements of actions */
        struct rule *nextrule;  /* pointer to next rule */
};
#define RULE struct rule
EXTERN RULE *Rules,             /* rule structures linked list head */
        *Rulep;                 /* working pointer */


/**********************************************************
 * Miscellaneous                                          *
 **********************************************************/
/*
 * Error exit values (returned to command shell)
 */
#define USAGE_ERROR     1       /* error in invokation */
#define FILE_ERROR      2       /* file not found errors */
#define RE_ERROR        3       /* bad regular expression */
#define ACT_ERROR       4       /* bad C action stmt */
#define MEM_ERROR       5       /* out of memory errors */
/*
 * Function  prototypes
 */

/* bawk.c */

_PROTOTYPE(int main , (int argc , char **argv ));
_PROTOTYPE(int compile , (void));
_PROTOTYPE(int process , (void));
_PROTOTYPE(int parse , (char *str , char *wrdlst [], char *delim ));
_PROTOTYPE(int unparse , (char *wrdlst [], int wrdcnt , char *str , char *delim ));
_PROTOTYPE(int instr , (int c , char *s ));
_PROTOTYPE(char *getmem , (unsigned len ));
_PROTOTYPE(char *newfile , (char *s ));
_PROTOTYPE(int strfile , (char *s ));
_PROTOTYPE(int getline , (void));
_PROTOTYPE(int getcharacter , (void));
_PROTOTYPE(int ungetcharacter , (int c ));
_PROTOTYPE(int endfile , (void));
_PROTOTYPE(int error , (char *s , int severe ));
_PROTOTYPE(int usage , (void));
_PROTOTYPE(int movemem , (char *from , char *to , int count ));
_PROTOTYPE(int fillmem , (char *array , int count , int value ));
_PROTOTYPE(int num , (int c ));
_PROTOTYPE(int alpha , (int c ));
_PROTOTYPE(int alphanum , (int c ));

/* bawkact.c */

_PROTOTYPE(int act_compile , (char *actbuf ));
_PROTOTYPE(int pat_compile , (char *actbuf ));
_PROTOTYPE(int stmt_compile , (char *actbuf ));
_PROTOTYPE(char *str_compile , (char *str , int delim ));
_PROTOTYPE(int getoken , (void));

/* bawkdo.c */

_PROTOTYPE(INT dopattern , (char *pat ));
_PROTOTYPE(int doaction , (char *act ));
_PROTOTYPE(int expression , (void));
_PROTOTYPE(int expr1 , (void));
_PROTOTYPE(int expr2 , (void));
_PROTOTYPE(int expr3 , (void));
_PROTOTYPE(int expr4 , (void));
_PROTOTYPE(int expr5 , (void));
_PROTOTYPE(int expr6 , (void));
_PROTOTYPE(int expr7 , (void));
_PROTOTYPE(int expr8 , (void));
_PROTOTYPE(int expr9 , (void));
_PROTOTYPE(int expr10 , (void));
_PROTOTYPE(int primary , (void));
_PROTOTYPE(int preincdec , (void));
_PROTOTYPE(int postincdec , (void));
_PROTOTYPE(int statement , (void));
_PROTOTYPE(int skipstatement , (void));
_PROTOTYPE(int skip , (int left , int right ));
_PROTOTYPE(int syntaxerror , (void));

/* bawkpat.c */

_PROTOTYPE(int re_compile , (char *patbuf ));
_PROTOTYPE(char *cclass , (char *patbuf ));
_PROTOTYPE(int match , (char *line , char *pattern ));
_PROTOTYPE(char *pmatch , (char *linestart , char *line , char *pattern ));

/* bawksym.c */

_PROTOTYPE(int isfunction , (char *s ));
_PROTOTYPE(int iskeyword , (char *s ));
_PROTOTYPE(int function , (int funcnum ));
_PROTOTYPE(VARIABLE *findvar , (char *s ));
_PROTOTYPE(VARIABLE *addvar , (char *name ));
_PROTOTYPE(int declist , (void));
_PROTOTYPE(VARIABLE *decl , (int type ));
_PROTOTYPE(int assignment , (void));
_PROTOTYPE(INT pop , (void));
_PROTOTYPE(int push , (int pclass , int plvalue , int psize , DATUM *pdatum ));
_PROTOTYPE(int pushint , (INT intvalue ));
_PROTOTYPE(INT popint , (void));
_PROTOTYPE(int pprint , (DATUM args []));
_PROTOTYPE(int pprntf , (char *fmt , DATUM args []));
