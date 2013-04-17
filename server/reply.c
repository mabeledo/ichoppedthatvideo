/* Reply module.
 * File: reply.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Allow other modules to create HTTP compliant replies.
 * */

#define _GNU_SOURCE

#include <string.h>
#include <time.h>

#include "common.h"
#include "msg.h"
#include "logging.h"
#include "reply.h"

/* ********** Constant definitions ********** */
#define SERVER			     "ichoppedthatvideo"
#define SERVER_LEN		     5
#define VERSION			     "4.6B1"
#define VERSION_LEN		     5

#define DATE_FMT		     "%a, %d %b %Y %H:%M:%S GMT"
#define DATE_LEN		     30

#define DEFAULT_HTTP_COMMAND	     ""
#define DEFAULT_HTTP_COMMAND_LEN     0
#define DEFAULT_HTTP_CODE	     "200 OK"
#define DEFAULT_HTTP_CODE_LEN	     6
#define DEFAULT_CONN_TYPE	     "closed"
#define DEFAULT_CONN_TYPE_LEN	     6
#define DEFAULT_EXPIRATION_TIME	     "-1"
#define DEFAULT_EXPIRATION_TIME_LEN  2
#define DEFAULT_CACHE_CONTROL	     "no-cache"
#define DEFAULT_CACHE_CONTROL_LEN    8
#define DEFAULT_PRAGMA_DIR	     "no-cache"
#define DEFAULT_PRAGMA_DIR_LEN	     8
#define DEFAULT_CONTENT_TYPE	     "text/html"
#define DEFAULT_CONTENT_TYPE_LEN     9

/* ********** Global variables ********** */

/* HTTP/1.1 chunked template.
 * Add-on to the HTTP/1.1 template to add chunked transfers.
 * */
const
char transfer_enc_header [] = "Transfer-Encoding: ";

const
int transfer_enc_header_len = 19;

/* HTTP/1.1 length template.
 * As length info cannot be inserted in the HTTP header when doing
 * 'chunked' transfers, this length template is splitted from the 
 * http complete template.
 * */
const
char content_len_header [] = "Content-Length: ";

const
int content_len_header_len = 16;

/* HTTP/1.1 Template.
 * This template contains all the necessary elements to compose a
 * HTTP response.
 * */
const
char http_template [] = "HTTP/1.1 %s\r\n"
			"Server: %s\r\n"
			"Date: %s\r\n"
			"Content-Type: %s\r\n"
			"%s\r\n"
			"Connection: %s\r\n"
			"Cache-Control: %s\r\n"
			"Expires: %s\r\n\r\n";
				  
const
size_t http_template_len = 91;

/* HTML Template.
 * */
const
char html_template [] = "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html\">"
			"<TITLE>%s</TITLE></HEAD><BODY>"
			"%s"
			"</BODY></HTML>\r\n\r\n";
const
size_t html_template_len = 110;

/* ********** Public functions ********** */

/* Specific replies.
 * 
 * Yes, this is a very poor programming practice. Functions should be
 * splitted in four (three for reply type and one to do the rest).
 * */

/* ok_reply()
 * 
 * */
char *
ok_reply			()
{
    ReplyParams params;
    char * res, * contents;
    unsigned int contents_len, reply_size;
	
    /* Initialize reply parameters. */
    params.http_command = NULL;
    params.http_code = HTTP_OK_CODE;
    params.content_type = HTML_TYPE;
    params.connection = CONN_CLOSE;
    params.transfer_encoding = NULL;
    params.cache_control = NO_CACHE;
    params.expiration = NO_EXPIRE;
	
    if ((contents_len = asprintf(&contents, html_template,
				 HTTP_OK_CODE, HTTP_OK_MSG)) == -1)
    {
	if (contents)
	{
	    free(contents);
	}
		
	log_message(WARNING, EMSG_COMPNOTFOUND, NULL);
	return (NULL);
    }
	
    res = compose_reply(params, (uint8_t *)contents, contents_len, &reply_size);

    free(contents);
    return (res);
}

/* serv_unavail_reply()
 * 
 * */
char *
serv_unavail_reply			()
{
    ReplyParams params;
    char * res, * contents;
    unsigned int contents_len, reply_size;
	
    /* Initialize reply parameters. */
    params.http_command = NULL;
    params.http_code = HTTP_SERVICE_UNAVAIL_CODE;
    params.content_type = HTML_TYPE;
    params.connection = CONN_CLOSE;
    params.transfer_encoding = NULL;
    params.cache_control = NO_CACHE;
    params.expiration = NO_EXPIRE;
	
    if ((contents_len = asprintf(&contents, html_template,
				 HTTP_SERVICE_UNAVAIL_CODE, HTTP_SERVICE_UNAVAIL_MSG)) == -1)
    {
	if (contents)
	{
	    free(contents);
	}
		
	log_message(WARNING, EMSG_COMPNOTFOUND, NULL);
	return (NULL);
    }
	
    res = compose_reply(params, (uint8_t *)contents, contents_len, &reply_size);

    free(contents);
    return (res);
}

/* not_found_reply()
 * 
 * */
char *
not_found_reply				()
{
    ReplyParams params;
    char * res, * contents;
    unsigned int contents_len, reply_size;
	
    /* Initialize reply parameters. */
    params.http_command = NULL;
    params.http_code = HTTP_NOT_FOUND_CODE;
    params.content_type = HTML_TYPE;
    params.connection = CONN_CLOSE;
    params.transfer_encoding = NULL;
    params.cache_control = NO_CACHE;
    params.expiration = NO_EXPIRE;
	
    if ((contents_len = asprintf(&contents, html_template,
				 HTTP_NOT_FOUND_CODE, HTTP_NOT_FOUND_MSG)) == -1)
    {
	if (contents)
	{
	    free(contents);
	}
		
	log_message(WARNING, EMSG_COMPNOTFOUND, NULL);
	return (NULL);
    }
	
    res = compose_reply(params, (uint8_t *)contents, contents_len, &reply_size);

    free(contents);
    return (res);
}

/* General purpose functions.
 * */

/* compose_reply()
 * 
 * Compose a reply using a set of parameters and a string with the desired
 * contents.
 * The returned value should be freed after its use with free().
 * Use 'uint8_t' for contents, as it can be fairly used with binary data.
 * 'reply_size' has the returned string length.
 * */
char *
compose_reply			(ReplyParams params, uint8_t * content, int content_len, unsigned int * reply_size)
{
	/* Here, 'var_field' is a field that should contain either a
	 * 'Transfer-encoding: chunked' or a 'Content-length: x' string.
	 * */
	char * fst_field, * expiration, * server, * date_time, * res, * var_field;
	int size, expiration_len, var_field_len;
	
	const time_t current_time = time(NULL);
	
	/* Check if this will be a HTTP response or a HTTP command.*/
	if ((params.http_command == NULL) || (strcmp(params.http_command, "") == 0))
	{
		fst_field = params.http_code;
	}
	else
	{
		fst_field = params.http_command;
	}
	
	/* Calculate the size of the reply and get rid of possible format
	 * errors.
	 * */
	size = http_template_len;
	
	if (fst_field != NULL)
	{
		size += strlen(fst_field);
	}
	else
	{
		fst_field = DEFAULT_HTTP_CODE;
		size += DEFAULT_HTTP_CODE_LEN;
	}
	
	if (params.content_type != NULL)
	{
		size += strlen(params.content_type);
	}
	else
	{
		params.content_type = DEFAULT_CONTENT_TYPE;
		size += DEFAULT_CONTENT_TYPE_LEN;
	}
	
	if (params.connection != NULL)
	{
		size += strlen(params.connection);
	}
	else
	{
		params.connection = DEFAULT_CONN_TYPE;
		size += DEFAULT_CONN_TYPE_LEN;
	}
	
	if (params.cache_control != NULL)
	{
		size += strlen(params.cache_control);
	}
	else
	{
		params.cache_control = DEFAULT_CACHE_CONTROL;
		size += DEFAULT_CACHE_CONTROL_LEN;
	}
	
	if (((expiration_len = asprintf(&expiration, "%d", params.expiration)) > 0) &&
		(expiration != NULL))
	{
		size += expiration_len;
	}
	else
	{
		expiration = DEFAULT_EXPIRATION_TIME;
		size += DEFAULT_EXPIRATION_TIME_LEN;
	}
	
	/* Compose a chunked transfer header. */
	if (params.transfer_encoding != NULL)
	{
		var_field_len = asprintf(&var_field, "%s%s", transfer_enc_header, params.transfer_encoding); 
		size += var_field_len;
	}
	else
	{
		/* Compose a regular header with 'Content-length' field, so
		 * calculate total reply_size.
		 * Note that the method used is not as good as it should be.
		 * */
		size += asprintf(&var_field, "%s%d", content_len_header, content_len);
	}
	
	/* Constant fields: 'Server' and 'Date' */
	size += asprintf(&server, "%s/%s", SERVER, VERSION);
	date_time = malloc(DATE_LEN * sizeof(char));
	size += strftime(date_time, DATE_LEN, DATE_FMT, localtime(&current_time));
	
	/* Finally, compose the reply. */
	res = malloc(size + content_len + 1);
	memset(res, '\0', size + content_len + 1);
	
	sprintf(res, http_template, fst_field, server, date_time, params.content_type, var_field,
								params.connection, params.cache_control, expiration);
	memcpy(res + size, content, content_len);
	
	free(date_time);
	free(var_field);
	
	if (expiration)
	{
		free(expiration);
	}

	if (reply_size != NULL)
	{
		*reply_size = size + content_len;
	}
	
	return (res);
}
