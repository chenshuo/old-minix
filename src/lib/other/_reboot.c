/* reboot.c - Systemcall interface to mm/signal.c::do_reboot()

   author: Edvard Tuinder  v892231@si.hhs.NL

   possible flags:
       HALT: let the system hang to power off the system
       REBOOT: reboot the system
 */

#include <lib.h>
#define reboot	_reboot
#include <unistd.h>

#define DO_HALT    0
#define DO_REBOOT  1

int reboot(how)
int how;
{
	message m;

	if (how != DO_HALT && how != DO_REBOOT)
		return EINVAL;

	m.m1_i1 = how;

	/* Clear unused fields */
	m.m1_i2 = 0;
	m.m1_i3 = 0;
	m.m1_p1 = NULL;
	m.m1_p2 = NULL;
	m.m1_p3 = NULL;

	return _syscall(MM, REBOOT, &m);
}
