/*
assert.h
*/
#ifndef ASSERT_H
#define ASSERT_H

#if DEBUG

#define INIT_PANIC()	static char *panic_warning_file= __FILE__

#define PANIC(print_list)  \
	( \
		printf("panic at %s, %d: ", panic_warning_file, __LINE__), \
		printf print_list, \
		printf("\n"), \
		panic(NULL, NO_NUM), \
		0 \
	)

#define WARNING(print_list)  \
	( \
		printf("warning at %s, %d: ", panic_warning_file, __LINE__), \
		printf print_list, \
		printf("\n"), \
		0 \
	)

#define assert(x) (!(x) ? (PANIC (( "assertion failed" )) ,0) : 0)
#define assertN(n,x) (!(x) ? (PANIC (( "assertion %d failed", n )),0) : 0)
#define compare(a,t,b) (!((a) t (b)) ? (PANIC(( "compare failed (%d, %d)", \
	a, b )),0) : 0)
#define compareN(n,a,t,b) (!((a) t (b)) ? (PANIC(( \
	"compare %d failed (%d, %d)", (n), (a), (b) )),0) : 0)

#else /* !DEBUG */

#define assert(x) (void)0
#define assertN(n,x) 0
#define compare(a,t,b) 0
#define compareN(n,a,t,b) 0

#endif /* DEBUG, !DEBUG */

#endif /* ASSERT_H */
