/* expr(1)  --  author Erik Baalbergen */

/* expr accepts the following grammar:
	expr ::= primary | primary operator expr ;
	primary ::= '(' expr ')' | signed-integer ;
  where the priority of the operators is taken care of.
  The resulting value is printed on stdout.
  Note that the ":"-operator is not implemented.
*/

#define EOI	0
#define OR	1
#define AND	2
#define LT	3
#define LE	4
#define EQ	5
#define NE	6
#define GE	7
#define GT	8
#define PLUS	9
#define MINUS	10
#define TIMES	11
#define DIV	12
#define MOD	13
#define COLON	14
#define LPAREN	15
#define RPAREN	16
#define OPERAND	20

#define MAXPRIO	6

struct op {
	char *op_text;
	short op_num, op_prio;
} ops[] = {
	{"|",	OR,	6},
	{"&",	AND,	5},
	{"<",	LT,	4},
	{"<=",	LE,	4},
	{"=",	EQ,	4},
	{"!=",	NE,	4},
	{">=",	GE,	4},
	{">",	GT,	4},
	{"+",	PLUS,	3},
	{"-",	MINUS,	3},
	{"*",	TIMES,	2},
	{"/",	DIV,	2},
	{"%",	MOD,	2},
	/* {":",	COLON,	1}, */
	{"(",	LPAREN,	-1},
	{")",	RPAREN,	-1},
	{0, 0, 0}
};

long eval(), expr();
char *prog;
char **ip;
struct op *ip_op;

main(argc, argv)
	char *argv[];
{
	long res;

	prog = argv[0];
	ip = &argv[1];
	res = expr(lex(*ip), MAXPRIO);
	if (*++ip != 0)
		syntax();
	printf("%D\n", res);
	exit(0);
}

lex(s)
	register char *s;
{
	register struct op *op = ops;

	if (s == 0) {
		ip_op = 0;
		return EOI;
	}
	while (op->op_text) {
		if (strcmp(s, op->op_text) == 0) {
			ip_op = op;
			return op->op_num;
		}
		op++;
	}
	ip_op = 0;
	return OPERAND;
}

long
num(s)
	register char *s;
{
	long l = 0;
	long sign = 1;

	if (*s == '\0')
		syntax();
	if (*s == '-') {
		sign = -1;
		s++;
	}
	while (*s >= '0' && *s <= '9')
		l = l * 10 + *s++ - '0';
	if (*s != '\0')
		syntax();
	return sign * l;
}

syntax()
{
	write(2, prog, strlen(prog));
	write(2, ": syntax error\n", 15);
	exit(1);
}

long
expr(n, prio)
{
	long res;

	if (n == EOI)
		syntax();
	if (n == LPAREN) {
		if (prio == 0) {
			res = expr(lex(*++ip), MAXPRIO);
			if (lex(*++ip) != RPAREN)
				syntax();
		}
		else
			res = expr(n, prio - 1);
	}
	else
	if (n == OPERAND) {
		if (prio == 0)
			return num(*ip);
		res = expr(n, prio - 1);
	}
	else
		syntax();
	while ((n = lex(*++ip)) && ip_op && ip_op->op_prio == prio)
		res = eval(res, n, expr(lex(*++ip), prio - 1));
	ip--;
	return res;
}

long
eval(l1, op, l2)
	long l1, l2;
{
	switch (op) {
	case OR:
		return l1 ? l1 : l2;
	case AND:
		return (l1 && l2) ? l1 : 0;
	case LT:
		return l1 < l2;
	case LE:
		return l1 <= l2;
	case EQ:
		return l1 == l2;
	case NE:
		return l1 != l2;
	case GE:
		return l1 >= l2;
	case GT:
		return l1 > l2;
	case PLUS:
		return l1 + l2;
	case MINUS:
		return l1 - l2;
	case TIMES:
		return l1 * l2;
	case DIV:
		return l1 / l2;
	case MOD:
		return l1 % l2;
	}
	fatal();
}

fatal()
{
	write(2, "fatal\n", 6);
	exit(1);
}
