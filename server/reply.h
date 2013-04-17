/* Reply module.
 * File: reply.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Allow other modules to create HTTP compliant replies.
 * */
 
#ifndef REPLY_H
#define REPLY_H

/* ********** Constant definitions ********** */

/* HTTP Commands.
 * Available HTTP commands. Actually, only 'GET' is supported.
 * */
#define HTTP_GET_COMMAND		"GET"

/* HTTP Status codes and messages.
 * HTTP_*_CODE definitions contain HTTP standard response codes.
 * HTTP_*_MSG definitions contain human readable messages.
 * */
#define HTTP_OK_CODE			"200 OK"
#define HTTP_ACCEPTED_CODE		"202 Accepted"
#define HTTP_BAD_REQUEST_CODE		"400 Bad Request"
#define HTTP_FORBIDDEN_CODE		"403 Forbidden"	
#define HTTP_NOT_FOUND_CODE		"404 Not Found"
#define HTTP_TOO_LONG_CODE		"414 Request-URI Too Long"
#define HTTP_SERVER_ERROR_CODE		"500 Internal Server Error"
#define HTTP_SERVICE_UNAVAIL_CODE	"503 Service Unavailable"

#define HTTP_OK_MSG			"<h1>Request OK</h1>"
#define HTTP_ACCEPTED_MSG		"<h1>Request accepted, wait to complete the process...</h1>"
#define HTTP_BAD_REQUEST_MSG		"<h1>Bad request, please check your URI</h1>"
#define HTTP_FORBIDDEN_MSG		"<h1>Forbidden, you do not have access to this place</h1>"	
#define HTTP_NOT_FOUND_MSG		"<h1>Object not found</h1>"
#define HTTP_TOO_LONG_MSG		"<h1>Your request URI is too long</h1>"
#define HTTP_SERVER_ERROR_MSG		"<h1>Internal error, please come back later</h1>"
#define HTTP_SERVICE_UNAVAIL_MSG	"<h1>Service temporarily unavailable</h1>"

/* Connection types. */
#define CONN_CLOSE			"close"
#define CONN_KEEP			"Keep-Alive"

/* Transfer encoding. */
#define CHUNKED				"chunked"

/* Expiration time. */
#define NO_EXPIRE			-1

/* Cache control. */
#define NO_CACHE			"no-cache"
#define MAX_AGE				"max-age="

/* Supported content types. */
#define XML_TYPE			"application/xml"
#define SWF_TYPE			"application/x-shockwave-flash"
#define HTML_TYPE			"text/html"
#define PLAIN_TYPE			"text/plain"
#define FLV_TYPE			"video/x-flv"
#define MP4_TYPE			"video/mp4"

/* ********** Type definitions ********** */
struct _reply_params
{
    char * http_command;
    char * http_code;
    char * content_type;
    char * connection;
    char * transfer_encoding;
    char * cache_control;
    int expiration;
};

/* Only a small note here: Hungarian notation is only used with type
 * definitions to avoid '_' separator in new types.
 * */
typedef struct _reply_params ReplyParams;

/* ********** Public functions ********** */
/* Specific replies.
 * */
char *
ok_reply			();

char *
serv_unavail_reply		();

char *
not_found_reply			();

/* General purpose functions.
 * */

char *
compose_reply			(ReplyParams params, uint8_t * content, int content_len, unsigned int * reply_size);

#endif
