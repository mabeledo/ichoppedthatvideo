/* File module.
 * File: file.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Non video files related module.
 * */

#define _GNU_SOURCE
#define _BSD_SOURCE

#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "request.h"
#include "common.h"
#include "stream.h"
#include "reply.h"

#include "file.h"

/* ********** Constant definitions ********** */

/* Per ad available files. */
#define CROSSDOMAIN_FILE_NAME		"crossdomain.xml"
#define DATA_FILE_NAME			"data.xml"
#define INTRO_FILE_NAME			"intro.swf"
#define PLAYER_FILE_NAME		"player.swf"
#define ROBOTS_FILE_NAME                "robots.txt"
#define ADJS_FILE_NAME			"sfMovie.js"

#define CROSSDOMAIN_FILE_CODE		0
#define DATA_FILE_CODE			1
#define INTRO_FILE_CODE			2
#define PLAYER_FILE_CODE		3
#define ROBOTS_FILE_CODE                4
#define ADJS_FILE_CODE			5


/* Default paths. */
#define DEFAULT_PATH			"/etc/ichoppedthatvideo/"
#define DEFAULT_CROSSDOMAIN_PATH	"/etc/ichoppedthatvideo/crossdomain.xml"
#define DEFAULT_DATA_PATH		"/etc/ichoppedthatvideo/data.xml"
#define DEFAULT_INTRO_PATH		"/etc/ichoppedthatvideo/intro.swf"
#define DEFAULT_PLAYER_PATH		"/etc/ichoppedthatvideo/player.swf"
#define DEFAULT_ROBOTS_PATH             "/etc/ichoppedthatvideo/robots.txt"
#define DEFAULT_ADJS_PATH		"/etc/ichoppedthatvideo/sfMovie.js"

/* File types arrays. */
const
char * files_supported [] = {CROSSDOMAIN_FILE_NAME, DATA_FILE_NAME,
			     INTRO_FILE_NAME, PLAYER_FILE_NAME,
			     ROBOTS_FILE_NAME, ADJS_FILE_NAME};

const
char * default_paths [] = {DEFAULT_CROSSDOMAIN_PATH, DEFAULT_DATA_PATH,
			   DEFAULT_INTRO_PATH, DEFAULT_PLAYER_PATH,
			   DEFAULT_ROBOTS_PATH, DEFAULT_ADJS_PATH};

const
char * http_types [] = {XML_TYPE, XML_TYPE, SWF_TYPE, SWF_TYPE, PLAIN_TYPE, HTML_TYPE};

const int files_supported_num = 6;



/* ********** Global variables ********** */
char * ad_path = NULL;


/* ********** Private functions ********** */

/* check_supported_file()
 * 
 * Checks if the directory, the file 'filename' and its type are supported.
 * Returns the type in 'reply.h' fashion, that must be freed after use.
 * */
char *
check_supported_file		(char * filename)
{
    char * res;
    int i;
    const int vlen = 5;

    /* Find out if it is a valid file name. */
    for (i = 0; (i < vlen) && (strcmp(files_supported[i], filename) != 0); i++);
    if (i >= vlen)
    {
	return (NULL);
    }
	
    /* Get the file type. */
    if (asprintf(&res, "%s", http_types[i]) <= 0)
    {
	return (NULL);
    }
	
    return (res);
}

/* ********** Public functions ********** */

/* init_file()
 * 
 * Initialize some global variables needed to use per ad files.
 * */
int
init_file					(char * path)
{
    if (!check_file_exists(path) ||
	(asprintf(&ad_path, "%s", path) <= 0))
    {
	log_message(CRITICAL, EMSG_ADPATH, NULL);
	return (ECOD_ADPATH);
    }
	
    return (EXIT_SUCCESS);
}

/* get_file_by_id()
 * 
 * Returns contents file (as char *) using some URL parameters.
 * The returned value should be freed after its use with free().
 * If there is no ad with the id provided, return the default one.
 * */
char *
get_file_by_id			(char * filename, char ** params)
{
    char * res, * path;
    uint8_t * contents;
    ReplyParams reply;
    int i;
	
    /* Initialize constant reply parameters. */
    reply.http_command = NULL;
    reply.http_code = HTTP_OK_CODE;
    reply.connection = CONN_CLOSE;
    reply.transfer_encoding = NULL;
    reply.expiration = NO_EXPIRE;

    /* Check if file type and name are supported. */
    if ((reply.content_type = check_supported_file(filename)) == NULL)
    {
	log_message(ERROR, EMSG_FILEUNKNOWN, filename);
	return (NULL);
    }
	
    if (params[ichoppedthatvideoD_PARAM_CODE] != NULL)
    {
	/* Manage requests with arguments. */ 
	if (check_supported_dir(ad_path, params[ichoppedthatvideoD_PARAM_CODE]) == 0)
	{
	    log_message(ERROR, EMSG_ADIDUNK, params[ichoppedthatvideoD_PARAM_CODE]);
	    return (NULL);
	}
		
	/* Build the file path. */
	if (asprintf(&path, "%s/%s/%s", ad_path, params[ichoppedthatvideoD_PARAM_CODE], filename) <= 0)
	{
	    log_message(ERROR, EMSG_BUILDPATH, filename);
	    return (NULL);
	}
    }
    else
    {
	/* Manage generic files, such as crossdomain.xml or robots.txt. */
	for (i = 0; (i < files_supported_num) && (strcmp(filename, files_supported[i])); i++);

	if (i < files_supported_num)
	{
	    asprintf(&path, "%s", default_paths[i]);
	}
	else
	{
	    log_message(ERROR, EMSG_FILEUNKNOWN, filename);
	    return (NULL);
	}
    }
	
    /* Check if file exists. */
    if (!check_file_exists(path))
    {
	log_message(ERROR, EMSG_FILEPATH, path);
	free(path);
	return (NULL);
    }

    contents = get_file_contents(path);
	
    /* Cache control. */
    if (params[CACHE_PARAM_CODE] != NULL)
    {
	asprintf(&reply.cache_control, "%s%s", MAX_AGE, params[CACHE_PARAM_CODE]);
    }
    else
    {
	reply.cache_control = NO_CACHE;
    }

    /* 'data.xml' is a special case: it has a line that should be changed
     * before composing the reply.
     * */
    if (strstr(path, DATA_FILE_NAME))
    {
	find_and_replace((char**)&contents, "SERVER_URL", get_server_name());
    }
	
    res = compose_reply(reply, contents, strlen((char*)contents), NULL);
    free(contents);
    free(path);
    free(reply.content_type);
	
    if (params[CACHE_PARAM_CODE] != NULL)
    {
	free(reply.cache_control);
    }

    return (res);
}
