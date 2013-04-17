/* Statistics module.
 * File: stat.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * */

#ifndef STAT_H
#define STAT_H

/* ********** Public functions. ********** */

int
init_stat			(char * host, char * path);

int
new_request_stat		(unsigned long ip_num, const char * type, char * user_agent);

int
new_stream_stat			(int request_id, int video_id, int bytes);

#endif
