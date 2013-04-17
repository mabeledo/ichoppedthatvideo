/* Server connection module.
 * File: conn.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 *
 * Connection data, helper functions and main loop process.
 * */

#ifndef CONN_H
#define CONN_H

/* ********** Constant definitions ********** */
#define DEFAULT_PORT	        80
#define DEFAULT_NUM_CHILDREN    192
#define DEFAULT_CLOSED_TO       0

/* ********** Public functions ********** */
int
get_child_num			();

int
get_child_pos			();

long long *
get_conn_served			();

int
init_conn			(int port, int num_children, int closed_timeout);

#endif
