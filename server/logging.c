/* Logging module.
 * File: logging.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Manage logs and messages
 * */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#include "common.h"
#include "logging.h"

/* ********** Constant definitions ********** */
#define CONSOLE_CODE  1
#define SYSLOG_CODE   2
#define BOTH_CODE     4


/* ********** Global variables ********** */
int min_level  = 0;
int output_dev = 0;

/* ********** Public functions ********** */

/* init_log()
 * 
 * Sets logging options.
 *  - 'level', is the minimum message level logged.
 *  - 'output', is the output device/format. Can be 'console',
 *    'syslog' or 'both', but 'console' won't be available if
 *    the application is daemonized.
 * Function errors will be  
 * */
int
init_log		(int level, char * output)
{
    /* Minimum level needed to log a message. */
    if ((min_level = (uint)level) > 4)
    {
	fprintf(stderr, EMSG_INVALIDLEVEL);
	return (ECOD_INVALIDLEVEL);
    }

    /* Where messages will be written. */
    if (!(((strcmp(output, CONSOLE) == 0) && (output_dev = CONSOLE_CODE)) ||
	  ((strcmp(output, SYSLOG) == 0) && (output_dev = SYSLOG_CODE)) ||
	  ((strcmp(output, BOTH) == 0) && (output_dev = BOTH_CODE))))
    {
	fprintf(stderr, EMSG_INVALIDDEV);
	return (ECOD_INVALIDDEV);
    }

    return (EXIT_SUCCESS);
}

/* log_message()
 * 
 * */
void
log_message		(int level, char * msg, char * data)
{
    char * full_msg;
    int err_num = errno;
    int priority;

    /* Priority mask check.
     * If the minimum verbose level is above the level specified, nothing occurs.
     * */
    if (level >= min_level)
    {
	if (data != NULL)
	{
	    if (level > MESSAGE)
	    {
		asprintf(&full_msg, "[MSG] %s    [DATA] %s    [SYSMSG] %s", msg, data, strerror(err_num));
	    }
	    else
	    {
		asprintf(&full_msg, "[MSG] %s    [DATA] %s", msg, data);
	    }
	}
	else
	{
	    if (level > MESSAGE)
	    {
		asprintf(&full_msg, "[MSG] %s    [SYSMSG] %s", msg, strerror(err_num));
	    }
	    else
	    {
		asprintf(&full_msg, "[MSG] %s", msg);
	    }
	}

	/* Log to console. */
	if ((output_dev == BOTH_CODE) || (output_dev == CONSOLE_CODE))
	{
	    fprintf(stderr, "ichoppedthatvideo: %s", full_msg);
	}

	/* Log to syslog. */
	if ((output_dev == BOTH_CODE) || (output_dev == SYSLOG_CODE))
	{
	    priority = LOG_USER;

	    switch(level)
	    {
		case (CRITICAL):
		    priority = priority | LOG_CRIT;
		    break;
		case (ERROR):
		    priority = priority | LOG_ERR;
		    break;	
		case (WARNING):
		    priority = priority | LOG_WARNING;
		    break;
		case (MESSAGE):
		    priority = priority | LOG_NOTICE;
		    break;
		default:
		    priority = priority | LOG_INFO;
	    }

	    syslog(priority, "%s", full_msg);
	}

	free(full_msg);
    }
}
