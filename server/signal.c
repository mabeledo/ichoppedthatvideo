/* Signal module.
 * File: signal.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Signal manager.
 * */

#define _GNU_SOURCE

#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "conn.h"


/* ********** Public functions. ********** */

/* default_handler()
 * 
 * Takes care of signals that might stop the program, and restores
 * some configuration parameters to its old values.
 * */
void
default_handler			(int signum)
{		
    struct rlimit core_limit;

    switch(signum)
    {
	case (SIGSEGV):
	    log_message(CRITICAL, EMSG_SIGSEGV, NULL);
	    break;
	case (SIGTERM):
	    log_message(MESSAGE, IMSG_SIGTERM, NULL);
	    break;
	case (SIGINT):
	    log_message(MESSAGE, IMSG_SIGINT, NULL);
	    break;
	default:
	    log_message(MESSAGE, IMSG_SIGDEFAULT, NULL);
	    break;
    }

    core_limit.rlim_cur = 0;
    core_limit.rlim_max = 0;

    if (setrlimit(RLIMIT_CORE, &core_limit) != 0)
    {
	log_message(WARNING, EMSG_RULIMIT, NULL);
    }	

    printf("\n\nClosing process...\n");
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
    exit(signum);
}
