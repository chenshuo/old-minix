/*
 * tsort - do a topological sort on the ordered pairs of names
 *
 * syntax - tsort [ file ]
 *
 * based on the discussion in 'The AWK programming Language', by
 * Aho, Kernighan, & Weinberger.
 *	
 * author:	Monty Walls
 * written:	1/28/88
 * Copyright:	Copyright (c) 1988 by Monty Walls.
 *		Not derived from licensed software.
 *
 *		Permission to copy and/or distribute granted under the
 *		following conditions:
 *	
 *		1). This notice must remain intact.
 *		2). The author is not responsible for the consequences of use
 *			this software, no matter how awful, even if they
 *			arise from defects in it.
 *		3). Altered version must not be represented as being the 
 *			original software.
 *
 * change log:
 *	possible bug in ungetc(), fixed readone() to avoid - 2/19/88 - mrw
 *	massive design error, rewrote dump logic - 3/15/88 - mrw
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#define printmem(_s)	(fprintf(stdout,"%s ",(_s)))
#define MAXNAMELEN	32
#define MAXMEMBERS	1024

struct dependents {
	struct node *nd;
	struct dependents *next;
};

struct node {
	char *name;
	int pcnt, scnt, visited;
	struct dependents *succ, *pred;
	struct node *left, *right;
};

char *progname;

extern struct node *readnode(), *findnode();
extern char *malloc(), *xalloc(), *readone(), *strsave();
extern struct dependents *finddep();
extern void dumptree(), walktree(), emptytree(), addqueue(), walklist();

extern int errno;
extern char *sys_errlist[];

struct node *tree, *lastnode, *q[MAXMEMBERS];
struct dependents *lastdepd;
int node_cnt, rc, front, back;

main(argc, argv)
int argc;
char **argv;
{
	progname = argv[0];
	if (argc > 1) 
		if (freopen(argv[1], "r", stdin) == (FILE *)NULL) {
			fprintf(stderr,"Error: %s - %s\n",progname, sys_errlist[errno]);
			exit(1);
		}

	/* read in the tree of entries */
	while (readnode() != (struct node *)NULL) 
		;	
	dumptree(tree);
	fflush(stdout);

	if (node_cnt != back) {
		fprintf(stderr,"Error: %s - input contains a cycle\n",progname);
		rc = 1;
	}

	exit(rc);
}


struct node *
readnode()
{
	char *s1, *s2;
	register struct node *n1, *n2;
	struct dependents *pd;

	if ((s1 = readone()) != (char *)NULL) {
		if ((n1 = findnode(s1)) == (struct node *)NULL) {
			/* is a new node so build it */
			n1 = (struct node *)xalloc(sizeof(struct node));
			n1->name = strsave(s1);
			n1->succ = (struct dependents *)NULL;
			n1->pred = (struct dependents *)NULL;
			n1->left = (struct node *)NULL;
			n1->right = (struct node *)NULL;
			n1->pcnt = 0;
			n1->scnt = 0;
			n1->visited = 0;
			linknode(n1);
		}
		if ((s2 = readone()) != (char *)NULL) {
			if ((n2 = findnode(s2)) == (struct node *)NULL) {
				/* is a new node so build it */
				n2 = (struct node *)xalloc(sizeof(struct node));
				n2->name = strsave(s2);
				n2->succ = (struct dependents *)NULL;
				n2->pred = (struct dependents *)NULL;
				n2->left = (struct node *)NULL;
				n2->right = (struct node *)NULL;
				n2->pcnt = 0;
				n2->scnt = 0;
				n2->visited = 0;
				linknode(n2);
			}
			if (n1 != n2) {
				if (finddep(n2->pred,s1) == (struct dependents *)NULL) {
					/* new dependence here */
					pd = (struct dependents *)xalloc(sizeof(struct dependents));
					pd->nd = n1;
					pd->next = (struct dependents *)NULL;
					n2->pcnt++;
					if (n2->pred == (struct dependents *)NULL) 
						n2->pred = pd;
					else
						lastdepd->next = pd;
				}
				if (finddep(n1->succ,s2) == (struct dependents *)NULL) {
					/* new dependence here */
					pd = (struct dependents *)xalloc(sizeof(struct dependents));
					pd->nd = n2;
					pd->next = (struct dependents *)NULL;
					n1->scnt++;
					if (n1->succ == (struct dependents *)NULL) 
						n1->succ = pd;
					else
						lastdepd->next = pd;
				}
			}
			return (n1);
		}
		else
			return ((struct node *)NULL);
	}
	else
		return ((struct node *)NULL);
}

void
dumptree(top)
register struct node *top;
{
	walktree(top);		/* get all entries in order with no predecessors */
	for (front = 1; front <= back; front++) {
		printmem(q[front]->name);
		walklist(q[front]->succ);
	}
	emptytree(top);		/* dumps all isolated nodes left over */
}

void
walklist(s)
register struct dependents *s;
{
	for (; s; s = s->next) {
		if (--s->nd->pcnt == 0) {
			addqueue(s->nd);
			s->nd->visited = 1;
			walklist(s->nd->succ);
		}
	}
}

void
walktree(t)
register struct node *t;
{
	if (t) {
		node_cnt++;
		walktree(t->right);
		if (t->pcnt == 0) {
			addqueue(t);
			t->visited = 1;
		}
		walktree(t->left);
	}
}

void
emptytree(t)
register struct node *t;
{
	struct dependents *s;

	/* t  - represents a remaining entry which is in a cycle */
	if (t) {
		emptytree(t->right);
		if (t->visited == 0) {
			t->visited = 1;
			printmem(t->name);
			for (s = t->succ; s; s = s->next)
				if (s->nd->visited == 0) {
					fprintf(stderr,"Error: %s - %s and %s are in a cycle\n",progname, t->name, s->nd->name);
				}
		}
		emptytree(t->left);
	}
}

void
addqueue(t)
struct node *t;
{
	if (++back >= MAXMEMBERS) {
		fprintf(stderr,"Error: %s - member queue overflow\n",progname);
		exit(1);
	}
	else
		q[back] = t;
}

char *
readone()
{
	register int c, n = 0;
	static char name[MAXNAMELEN];

	/* eat up leading spaces */
	while ((c = getchar()) != EOF && isspace(c))
		;

	if (c != EOF) {
		name[n++] = c;	/* save into name first non blank */
		while ((c = getchar()) != EOF && !isspace(c)) {
			if (n < MAXNAMELEN)
				name[n++] = c;
		}
		name[n] = '\0';
		return (name);
	}
	else
		return ((char *)NULL);

}

struct node *
findnode(s)
char *s;
{
	register struct node *n;
	register int cmp;

	if (tree) {
		lastnode = n = tree;
		while (n && n->name) {
			lastnode = n;
			if (!(cmp = strcmp(s,n->name)))
				return (n);
			else if (cmp > 0)
				n = n->left;
			else
				n = n->right;
		}
	}
	return ((struct node *)NULL);
}

struct dependents *
finddep(dp, s)
register struct dependents *dp;
register char *s;
{
	lastdepd = (struct dependents *)NULL;
	while (dp && dp->nd) {
		lastdepd = dp;
		if (strcmp(dp->nd->name,s) == 0)
			return (dp);
		else {
			dp = dp->next;
		}
	}
	return ((struct dependents *)NULL);
}

linknode(n)
register struct node *n;
{
	register int cmp;

	if (tree) {
		cmp = strcmp(n->name,lastnode->name);
		if (cmp > 0)
			lastnode->left = n;
		else
			lastnode->right = n;
	}
	else
		tree = n;
}

char *
xalloc(n)
int n;
{
	char *p;
	
	if ((p = malloc(n)) != (char *)NULL)
		return (p);
	else {
		fprintf(stderr,"Error: %s out of memory\n",progname);
		exit(1);
	}
}

char *
strsave(s)
char *s;
{
	char *p;

	p = xalloc(strlen(s)+1);
	strcpy(p,s);
	return (p);
}
