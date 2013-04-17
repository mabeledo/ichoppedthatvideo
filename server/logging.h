/* Logging module.
 * File: logging.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Manage logs and messages
 * */

#ifndef LOGGING_H
#define LOGGING_H

/* ********** Constant definitions ********** */

#define DEFAULT_LOG_LEVEL  CRITICAL
#define DEFAULT_OUTPUT     "syslog"

/* Logging levels. */
#define CRITICAL           4
#define ERROR	           3
#define WARNING	           2
#define MESSAGE	           1

/* Logging devices. */
#define CONSOLE            "console"
#define SYSLOG             "syslog"
#define BOTH               "both"

/* ********** Public functions ********** */
int
init_log	(int level, char * output);

void
log_message	(int level, char * msg, char * data);

#endif
