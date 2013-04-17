/* File module.
 * File: file.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * File reading and writing.
 * */ 

#ifndef FILE_H
#define FILE_H

/* ********** Public functions ********** */
int
init_file				(char * filename);

int
save_common_info		(char * path, char * sign, int num_streams);

int
save_stream_info		(char * path, char * filename, int64_t * iframe_offset, int iframe_num);

int
exit_file				();

#endif
