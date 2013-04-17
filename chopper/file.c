/* File module.
 * File: file.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 *
 * File reading and writing.
 * */ 

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../server/common.h"
#include "file.h"


/* ********** Global variables ********** */
static
char * file;

/* ********** Public functions ********** */

/* init_file()
 * 
 * */
int
init_file		(char * filename)
{
    asprintf(&file, "%s", filename);
    return (TRUE);
}

/* save_common_info()
 * 
 * */
int
save_common_info	(char * path, char * sign, int num_streams)
{
    int fd, buffer_len;
    char * buffer, * full_name;
	
    asprintf(&full_name, "%s/%s", path, file);
	
    if ((fd = open(full_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
    {
	free(full_name);
	perror("Cannot open file");
	return (FALSE);
    }
	
    free(full_name);

    /* Write the directory path and the number of streams into it. */
    buffer_len = asprintf(&buffer, "%s\n%s\n%d\n", path, sign, num_streams);
    write(fd, buffer, buffer_len);
    free(buffer);

    close(fd);
    return (TRUE);	
}

/* save_stream_info()
 * 
 * */
int
save_stream_info	(char * path, char * filename, int64_t * iframe_offset, int iframe_num)
{
    int fd, i, buffer_len;
    char * buffer, * full_name;
	
    asprintf(&full_name, "%s/%s", path, file);
	
    if ((fd = open(full_name, O_RDWR | O_APPEND)) < 0)
    {
	free(full_name);
	perror("Cannot open file");
	return (FALSE);
    }
	
    free(full_name);
	
    /* Write the file name and the number of iframes first.*/
    buffer_len = asprintf(&buffer, "%s\n\%d\n", filename, iframe_num);
    write(fd, buffer, buffer_len);
    free(buffer);
	
    /* Write the iframe offsets. */
    for (i = 0; i < iframe_num; i++)
    {
	buffer_len = asprintf(&buffer, "%lld ", iframe_offset[i]);
	write(fd, buffer, buffer_len);
	free(buffer);
    }
	
    /* Close the last line and put a new line separator. */
    buffer_len = asprintf(&buffer, "\n");
    write(fd, buffer, buffer_len);
    free(buffer);
	
    close(fd);
    return (TRUE);
}

/* exit_file()
 * 
 * */
int
exit_file		()
{
    free(file);
    return (TRUE);
}
