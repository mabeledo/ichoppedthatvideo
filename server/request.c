/* Request module.
 * File: request.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Manage requests from clients.
 * This module would also call DB related functions to eventually save 
 * statistic data.
 * Note that there is some suboptimal functions used here, because of
 * a lot of string manipulation needed to parse requests.
 * */

#define _GNU_SOURCE

#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "common.h"
#include "conn.h"
#include "file.h"
#include "stream.h"
#include "reply.h"
#include "stat.h"
#include "security.h"
#include "request.h"

/* ********** Constant definitions ********** */

/* Define requests codes and names. */
#define CROSSDOMAIN_REQUEST_CODE	0
#define DATA_REQUEST_CODE		1
#define INTRO_REQUEST_CODE		2
#define PLAYER_REQUEST_CODE		3
#define ROBOTS_REQUEST_CODE             4
#define ADJS_REQUEST_CODE		5
#define STREAM_REQUEST_CODE		6
#define EMPTY_REQUEST_CODE		99

#define CROSSDOMAIN_REQUEST_NAME	"crossdomain.xml"
#define DATA_REQUEST_NAME		"data.xml"
#define INTRO_REQUEST_NAME		"intro.swf"
#define PLAYER_REQUEST_NAME		"player.swf"
#define ROBOTS_REQUEST_NAME             "robots.txt"
#define ADJS_REQUEST_NAME		"sfMovie.js"
#define STREAM_REQUEST_NAME		"stream"
#define EMPTY_REQUEST_NAME		""

#define BUFFER_SIZE			1024
#define LOG_SIZE			250
#define GET_COMMAND			"GET /"
#define GET_COMMAND_LEN			5
#define AGENT_HEADER			"User-Agent:"
#define AGENT_HEADER_LEN		12

/* ********** Type definitions ********** */
typedef
struct _request
{
	unsigned long ip_num;
	int type;
	int video_id;
	char ** params;
	char * user_agent;
	
	char input[BUFFER_SIZE];
	unsigned short input_len;
}
Request;

/* ********** Global variables ********** */
const
char * request_names [] = {CROSSDOMAIN_REQUEST_NAME, DATA_REQUEST_NAME,
			   INTRO_REQUEST_NAME, PLAYER_REQUEST_NAME,
			   ROBOTS_REQUEST_NAME, ADJS_REQUEST_NAME,
			   STREAM_REQUEST_NAME};

const
size_t request_vlen = 7;

const
char * req_param_names [] = {VIDEOID_PARAM_NAME, SETID_PARAM_NAME,
				CLIENTID_PARAM_NAME, DATA_PARAM_NAME,
				INTRO_PARAM_NAME, SIGN_PARAM_NAME,
				QUALITY_PARAM_NAME, POS_PARAM_NAME,
				CACHE_PARAM_NAME};
						 
const
int req_param_vlen = 9;

/* ********** Private functions ********** */

/* parse_request()
 * 
 * Parse the request analyzing the received message.
 * */
int
parse_request		(Request * client_req)
{
    char * path, * add_info, * type, * saveptr, * params;
    char * http_header, * agent_ptr;
    unsigned short path_len, type_len, params_len, agent_len;
	
    /* Show the URL that will be parsed. */
    add_info = wipe_special_chars(client_req->input, LOG_SIZE);
    log_message(MESSAGE, IMSG_PARSING, add_info);
    free(add_info);

    /* Check if the first line of the request is well formed.
     * - Length is at least 14 bytes, to contain a GET command, a path
     *   and a protocol (HTTP 1.1 in this case).
     * - The first bytes contain "GET /", to avoid malformed requests.
     * - There is not only a GET and HTTP version in the request, but
     *   also a set of other data (actually the referrer data).
     * */
    if ((client_req->input_len >= 14) &&
	(strstr(client_req->input, GET_COMMAND) == client_req->input) &&
	(http_header = strchr(client_req->input, '\r')) &&
	(*(http_header + 1) != '\0'))
    {
	/* Check which is the type requested. */
	path = client_req->input + GET_COMMAND_LEN;
	path_len = strchr(path, ' ') - path;

	/* Manage empty requests to provide clean replies. */
	if (path_len < 1)
	{
	    client_req->type = EMPTY_REQUEST_CODE;
	    client_req->params = NULL;
	}
	else
	{
	    /* Find the request type code. */
	    asprintf(&type, "%s", path);
	    type = strtok_r(type, "? ", &saveptr);

	    /* Find the request type code. */
	    if ((client_req->type = find_vector_first(type, request_names, request_vlen)) < 0)
	    {
		add_info = wipe_special_chars(client_req->input, LOG_SIZE);
		log_message(WARNING, EMSG_TYPEUNKNOWN, add_info);
		free(add_info);
		free(type);
		return (ECOD_TYPEUNKNOWN);
	    }
			
	    /* Get the URL parameter list. */
	    free(type);
	    type_len = strlen(request_names[client_req->type]);
	    params_len = path_len - (type_len + 1);
	    params = malloc((params_len + 1) * sizeof(char));
	    snprintf(params, params_len + 1, "%s", path + type_len + 1);
	    client_req->params = parse_key_value(params, req_param_names, req_param_vlen, "=&", PARAM_MAX_LEN);
	    free(params);
	}
		
	/* Get the user agent, if available. */
	if ((agent_ptr = strstr(http_header, AGENT_HEADER)) != NULL)
	{
	    agent_ptr += AGENT_HEADER_LEN;
	    agent_len = strchr(agent_ptr, '\r') - agent_ptr + 1;
	    client_req->user_agent = malloc(agent_len * sizeof(char));
	    snprintf(client_req->user_agent, agent_len, "%s", agent_ptr);
	}
    }
    else
    {
	add_info = wipe_special_chars(client_req->input, LOG_SIZE);
	log_message(WARNING, EMSG_BADFORMREQ, add_info);
	free(add_info);
	return (ECOD_BADFORMREQ);
    }
	
    return (EXIT_SUCCESS);
}

/* ********** Public functions ********** */

/* manage_request()
 * 
 * Main HTTP IO function.
 * Here the application will read a client request, classify it and send
 * a response. 
 * Also, it will act as a hub for all the tasks involving HTTP headers,
 * referrers and so on.
 * Remember that this function is called in a multithreaded environment!
 * Every piece of shared memory must be locked with lock_mutex() before
 * writing on it, and unlocked with unlock_mutex() after.
 * */
int
manage_request			(int client_sd)
{
    Request client_req;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    char * output_buf;
    int bytes_sent, res, stat_req_id, i;
	
    client_req.ip_num = 0;
    memset(client_req.input, '\0', BUFFER_SIZE);
    client_req.input_len = 0;
    client_addr_len = sizeof(struct sockaddr_in);
    client_req.user_agent = NULL;
	
    /* Get client ip number in network order. */
    if (getpeername(client_sd, (struct sockaddr *)&client_addr, &client_addr_len) != 0)
    {
	log_message(WARNING, EMSG_NOSOCKINFO, NULL);
    }
    else
    {
	client_req.ip_num = client_addr.sin_addr.s_addr;
    }
	
    /* Before processing the request, check if it comes from a
     * blacklisted client.
     * */
    if ((res = check_client(client_req.ip_num)) != EXIT_SUCCESS)
    {
	output_buf = serv_unavail_reply();
	bytes_sent = send(client_sd, output_buf, strlen(output_buf), MSG_NOSIGNAL);
	free(output_buf);
	return (res);
    }
	
    /* Start reading some data.
     * Read the request in one go.
     * */
    if ((client_req.input_len = recv(client_sd, client_req.input, BUFFER_SIZE, 0)) < 0)
    {
	log_message(WARNING, EMSG_READSOCKET, NULL);
	return (ECOD_READSOCKET);
    }

    /* It is time to analyze received data.
     * The application only takes care of "GET" commands, why more?
     * */
    if ((res = parse_request(&client_req)) < 0)
    {
	output_buf = not_found_reply();
	bytes_sent = send(client_sd, output_buf, strlen(output_buf), MSG_NOSIGNAL);
	free(output_buf);
	return (res);
    }
	
    /* Log received message, for the extremely paranoid, and save a 'request'
     * statistic.
     * */
    log_message(MESSAGE, IMSG_VALIDPARAM, NULL);
    stat_req_id = new_request_stat(client_req.ip_num, request_names[client_req.type], client_req.user_agent);
	
    /* If there is a video petition, redirect socket to send_video()
     * In other case, use get_ad_file_by_id() from ads.c to get the
     * file.
     * */
    switch (client_req.type)
    {
	case (STREAM_REQUEST_CODE):
	    /* Send video. */
	    if ((bytes_sent = send_video(client_sd, client_req.params)) < 0)
	    {
		/* Stream not found or failed streaming, send a "not found" page. */
		output_buf = not_found_reply();
		bytes_sent = send(client_sd, output_buf, strlen(output_buf), MSG_NOSIGNAL);
		free(output_buf);
	    }
	    else
	    {
		/* Save stream stats. */
		if (stat_req_id >= 0)
		{
		    new_stream_stat(stat_req_id, atoi(client_req.params[VIDEOID_PARAM_CODE]), bytes_sent);
		}
	    }
	    break;
			
	case (EMPTY_REQUEST_CODE):
	    /* Empty request. Reply with a "200 OK" code. */
	    output_buf = ok_reply();
	    bytes_sent = send(client_sd, output_buf, strlen(output_buf), MSG_NOSIGNAL);
	    free(output_buf);
	    break;
			
	default:
		
	    /* Assume there is a identified request type and it is a non video file. */
	    if ((output_buf = get_file_by_id((char *)request_names[client_req.type], client_req.params)) != NULL)
	    {
		bytes_sent = send(client_sd, output_buf, strlen(output_buf), MSG_NOSIGNAL);
	    }
	    else
	    {
		/* Ad not found, so send a "not found" page. */
		output_buf = not_found_reply();
		bytes_sent = send(client_sd, output_buf, strlen(output_buf), MSG_NOSIGNAL);
	    }
			
	    free(output_buf);
		
	    break;
    }
	
    /* Free memory, if allocated. */
    if (client_req.params != NULL)
    {
	for (i = 0; i < req_param_vlen; i++)
	{
	    if (client_req.params[i] != NULL)
	    {
		free(client_req.params[i]);
	    }
	}
	free(client_req.params);
    }
	
    if (client_req.user_agent != NULL)
    {
	free(client_req.user_agent);
    }
	
    return (EXIT_SUCCESS);
}
