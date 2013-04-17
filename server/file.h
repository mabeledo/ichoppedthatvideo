/* File module.
 * File: file.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Non video files related module.
 * */

#ifndef FILE_H
#define FILE_H

/* ********** Public functions ********** */
int
init_file				(char * path);

char *
get_file_by_id			(char * filename, char ** params);

#endif
