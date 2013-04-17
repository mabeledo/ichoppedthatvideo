/* Security module.
 * File: security.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Security related utilities.
 * */

#ifndef SECURITY_H
#define SECURITY_H

/* ********** Constant definitions ********** */
#define DEFAULT_REQ_LIMIT     10
#define DEFAULT_TIME_LIMIT    30
#define DEFAULT_BLCK_LEN      500

/* ********** Public functions ********** */
int
init_security			(int req_limit, int time_limit, int len);

int
check_client			(unsigned long ip_num);

#endif
