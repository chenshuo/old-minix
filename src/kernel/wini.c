/*	wini.c - choose a winchester driver.		Author: Kees J. Bot
 *								28 May 1994
 * Several different winchester drivers may be compiled
 * into the kernel, but only one may run.  That one is chosen here using
 * the boot variable 'hd'.
 */

#include "kernel.h"
#include "driver.h"


/* Map driver name to task function. */
struct hdmap {
  char		*name;
  task_t	*task;
} hdmap[] = {

#if ENABLE_AT_WINI
  { "at",	at_winchester_task	},
#endif

#if ENABLE_BIOS_WINI && _WORD_SIZE == 2
  { "bios",	bios_winchester_task	},
#endif

#if ENABLE_PS_WINI
  { "ps",	ps_winchester_task	},
#endif

#if ENABLE_ESDI_WINI
  { "esdi",	esdi_winchester_task	},
#endif

#if ENABLE_XT_WINI
  { "xt",	xt_winchester_task	},
#endif

  { NULL, NULL }
};


/*===========================================================================*
 *				sel_wini_task				     *
 *===========================================================================*/
PUBLIC task_t *sel_wini_task()
{
  /* Return the default or selected winchester task. */
  char *hd;
  struct hdmap *map;

  hd = k_getenv("hd");

  for (map = hdmap; map->name != NULL; map++) {
	if (hd == NULL || strcmp(hd, map->name) == 0) break;
  }
  return map->task;
}


/*===========================================================================*
 *				winchester_task				     *
 *===========================================================================*/
PUBLIC void winchester_task()
{
  task_t *wini;

  wini = sel_wini_task();

  if (wini != NULL) {
	/* Run the selected winchester task. */
	(*wini)();
  } else {
	/* No winchester driver, so run the dummy task. */
	nop_task();
  }
}
