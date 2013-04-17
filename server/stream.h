/* Stream module.
 * File: stream.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * FLV loading and stream related functions.
 * */

#ifndef STREAM_H
#define STREAM_H

/* ********** Constant definitions ********** */

/* Default timeout (in seconds) */
#define DEFAULT_TIMEOUT 300
#define MIN_TIMEOUT     60


/* ********** Public functions ********** */
int
init_videos		(char * path, int auth, int timeout);

int
send_video		(int client_sd, char ** params);

int
close_videos		();

#endif
