/* Server connection module.
 * File: conn.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 *
 * Children data, helper functions and main loop process.
 * */

#define _GNU_SOURCE

#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mysql.h>

#include "request.h"
#include "common.h"
#include "conn.h"

/* ********** Constant definitions ********** */
#define QUEUE_PER_CHILD		3

/* ********** Type definitions ********** */
typedef struct _child
{
    pthread_t thread_id;
    long long conn_count;
} Child;

typedef Child * ChildrenArray;

/* ********** Global variables ********** */
int 	        child_num	= 0;
ChildrenArray	children	= NULL;
pthread_mutex_t conn_lock 	= PTHREAD_MUTEX_INITIALIZER;
int 		server_sd	= 0;
struct linger   close_timeout   = {0, 0};

/* 'alive_flag' contains a flag to notify each thread if it should
 * stop the main process.
 * */
int		alive_flag	= 0;

/* ********** Private functions ********** */

/* atomic_add_child()
 * 
 * Wrapper for external modules to access atomic_add_int with private
 * variable child_num.
 * */
void
atomic_add_child		()
{
	atomic_add_int(&child_num);
}

/* atomic_del_child()
 * 
 * Wrapper for external modules to access atomic_add_int with private
 * variable child_num.
 * */
void
atomic_del_child		()
{
	atomic_sub_int(&child_num);
}

/* child_main()
 * 
 * Main child process.
 * */
void *
child_main				(void * arg)
{
    int client_sd;
    int pos;
    socklen_t client_len;
    struct sockaddr_in client_addr;
    char * add_info;
	
    pos = (int)arg;
    client_len = sizeof(struct sockaddr_in);
	
    /* Initialize MySQL threaded interaction. */
    mysql_thread_init();
	
    asprintf(&add_info, "Thread ID: %d\n", pos);
    log_message(MESSAGE, IMSG_THREADINIT, add_info);
    free(add_info);
	
    while (alive_flag > 0)
    {
	pthread_mutex_lock(&conn_lock);
	client_sd = accept(server_sd, (struct sockaddr *) &client_addr, &client_len);
	pthread_mutex_unlock(&conn_lock);
		
	/* Abort connection without waiting to send remaining data. */
	setsockopt(client_sd, SOL_SOCKET, SO_LINGER, &close_timeout, sizeof(struct linger));
	children[pos].conn_count++;
		
	/* Process request */
	manage_request(client_sd);
	close(client_sd);
    }
	
    /* End MySQL threaded interaction . */
    mysql_thread_end();
    return (NULL);
}

/* create_child()
 * 
 * */
int
create_child			(int pos)
{
	return (pthread_create(&children[pos].thread_id, NULL, &child_main, (void *) pos));
}

/* ********** Public functions ********** */

/* get_child()
 * 
 * Return the number of children available.
 * */
int
get_child_num			()
{
    return (child_num);
}

/* get_child_pos()
 * 
 * Return the current thread position in the 'children' array.
 * */
int
get_child_pos			()
{
    int i;
    pthread_t thread_id;
	
    thread_id = pthread_self();
    for (i = 0; (i < child_num) && (pthread_equal(children[i].thread_id, thread_id) == 0); i++);
	
    if (i >= child_num)
    {
	log_message(WARNING, EMSG_CHILDNOTFOUND, NULL);
	return (ECOD_CHILDNOTFOUND);
    }
    return (i);
}

/* get_conn_served()
 * 
 * Return the connections served by children.
 * */
long long *
get_conn_served			()
{
    long long * res;
    int i;
	
    if ((res = malloc(child_num * sizeof(long long))) != NULL)
    {
	for (i = 0; i < child_num; i++)
	{
	    res[i] = children[i].conn_count;
	}
    }
    return (res);
}

/* init_conn()
 * 
 * Open sockets, allocate memory for the Child array and create all the
 * threads.
 * */
int
init_conn		(int port, int num_children, int closed_timeout)
{
    int i;
    const int reuse = 1;
    struct sockaddr_in server_address;
    char * add_info;
	
    if ((children = malloc(num_children * sizeof(Child))) == NULL)
    {
	log_message(CRITICAL, EMSG_CHILDALLOC, NULL);
	return (ECOD_CHILDALLOC);
    }
	
    atomic_add_int(&alive_flag);
	
    /* Create the server socket. */
    if ((server_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	log_message(CRITICAL, EMSG_SOCKET, NULL);
	return(ECOD_SOCKET);
    }

    /* Set this to avoid TIME_WAIT problems reusing the socket. */
    if (setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
	log_message(WARNING, EMSG_REUSEADDR, NULL);
    }

    /* Sets the connection information and bind it. */
    memset((char *) &server_address, 0, sizeof(struct sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(server_sd, (struct sockaddr *) &server_address, sizeof(struct sockaddr_in)) < 0)
    {
	close(server_sd);
	log_message(CRITICAL, EMSG_BIND, NULL);
	return (ECOD_BIND);
    }

    /* Listen for incoming connections.
     * Stablishes a queue per children to avoid losing requests.
     * */
    if (listen(server_sd, num_children * QUEUE_PER_CHILD) < 0)
    {
	close(server_sd);
	log_message(CRITICAL, EMSG_LISTEN, NULL);
	return (ECOD_LISTEN);
    }
	
    asprintf(&add_info, "Listening port: %d; Active threads: %d\n", port, num_children);
    log_message(MESSAGE, IMSG_CONNINIT, add_info);
    free(add_info);

    /* Set linger timeout. */
    if (closed_timeout > 0)
    {
	close_timeout.l_onoff = 1;
	close_timeout.l_linger = closed_timeout;
    }
    else
    {
	close_timeout.l_onoff = 0;
    }

    for (i = 0; i < num_children; i++)
    {
	children[i].conn_count = 0;
		
	if (create_child(i) != 0)
	{
	    /* This error should be managed more precisely.
	     * Maybe it might go on with at least 1 thread.
	     * */
	    log_message(ERROR, EMSG_CREATECHILD, NULL);
	    return (ECOD_CREATECHILD);
	}
	else
	{
	    atomic_add_child();
	}
    }

    return (EXIT_SUCCESS);
}

/* close_conn()
 * 
 * Close remaining connections and free allocated memory.
 * */
int
close_conn		()
{
    int i;
    char * res;
	
    close(server_sd);
    atomic_sub_int(&alive_flag);
	
    /* Wait for every child. */
    for (i = 0; i < child_num; i++)
    {
	pthread_join(children[i].thread_id, (void **)&res);
	free(res);
    }
    free(children);
	
    return (EXIT_SUCCESS);
}
