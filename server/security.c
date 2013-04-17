/* Security module.
 * File: security.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Security related utilities.
 * */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>

#include "common.h"
#include "security.h"

/* ********** Constant definitions ********** */
#define NANOSEC_IN_SEC		1000000000

/* ********** Type definitions ********** */
typedef
struct _entry
{
    unsigned long ip_num;
    int64_t counter;
    int64_t timestamp;
}
Entry;

/* ********** Global variables ********** */
int sec_enabled          = 0;
pthread_mutex_t sec_lock = PTHREAD_MUTEX_INITIALIZER;
Entry ** backlog         = NULL;
int backlog_len          = 0;
int backlog_last         = 0;

Entry ** blacklist       = NULL;
int blacklist_len        = 0;
int blacklist_last       = 0;

/* Maximum number of request per second allowed. */
int max_request          = 0;

/* Maximum time (in nanoseconds) that an ip could be blacklisted. */
int64_t max_time         = 0;

/* ********** Private functions ********** */

/* compare_entry_ip()
 * 
 * Compare entries by ip number.
 * */
inline
int
compare_entry_ip			(const void * fst, const void * snd)
{
	Entry ** fst_entry = (Entry **) fst;
	Entry ** snd_entry = (Entry **) snd;

	return ((*fst_entry)->ip_num - (*snd_entry)->ip_num);
}

/* compare_entry_date()
 * 
 * Compare entries by date.
 * */
inline
int
compare_entry_date			(const void * fst, const void * snd)
{
	Entry ** fst_entry = (Entry **) fst;
	Entry ** snd_entry = (Entry **) snd;

	return ((*fst_entry)->timestamp - (*snd_entry)->timestamp);
}

/* ********** Public functions ********** */

/* init_security()
 * 
 * */
int
init_security			(int req_limit, int time_limit, int len)
{
    int i;
    char * add_info;

    backlog_len = len * 10;
    backlog = malloc(backlog_len * sizeof(Entry *));
	
    for (i = 0; i < backlog_len; i++)
    {
	backlog[i] = malloc(sizeof(Entry));
	backlog[i]->ip_num = ULONG_MAX;
	backlog[i]->counter = 0;
	backlog[i]->timestamp = 0;
    }

    blacklist_len = len;
    blacklist = malloc(blacklist_len * sizeof(Entry *));
	
    for (i = 0; i < blacklist_len; i++)
    {
	blacklist[i] = malloc(sizeof(Entry));
	blacklist[i]->ip_num = ULONG_MAX;
	blacklist[i]->counter = 0;
	blacklist[i]->timestamp = 0;
    }
	
    max_request = req_limit;
    max_time = (int64_t)time_limit * NANOSEC_IN_SEC;
	
    asprintf(&add_info, "Request limit: %d; Time limit: %d; Queue len: %d\n", req_limit, time_limit, len);
    log_message(MESSAGE, IMSG_SECENABLED, add_info);
    free(add_info);
    sec_enabled = 1;
    return (EXIT_SUCCESS);
}

/* check_client()
 * 
 * */
int
check_client			(unsigned long ip_num)
{
    char * ip;
    Entry * cur_entry, * tmp_entry, ** found_entry;
	
    if (!sec_enabled)
    {
	return (EXIT_SUCCESS);
    }
	
    tmp_entry = malloc(sizeof(Entry));
    tmp_entry->ip_num = ip_num;
    tmp_entry->timestamp = get_time();
    tmp_entry->counter = 1;
	
    pthread_mutex_lock(&sec_lock);
	
    /* Search for it on 'blacklist' list. */
    if ((found_entry = bsearch(&tmp_entry, blacklist, blacklist_len, sizeof(Entry *), compare_entry_ip)) != NULL)
    {
	cur_entry = *found_entry;
		
	/* Maybe this entry is too old. Remove if so. */
	if ((get_time() - cur_entry->timestamp) > max_time)
	{
	    cur_entry->ip_num = ULONG_MAX;
	    cur_entry->timestamp = ULONG_MAX;
	    cur_entry->counter = 0;
	    qsort(blacklist, blacklist_len, sizeof(Entry *), compare_entry_ip);
	}
	else
	{
	    /* Update fields and notify as "already blacklisted" */
	    cur_entry->counter++;
	    cur_entry->timestamp = get_time();
	    pthread_mutex_unlock(&sec_lock);
			
	    asprintf(&ip, "IP: %lu", ip_num);
	    log_message(ERROR, EMSG_BLACKLISTED, ip);
	    free(tmp_entry);
	    free(ip);
			
	    return (ECOD_BLACKLISTED);
	}
    }
	
    /* Search for it on 'backlog' list. */
    if ((found_entry = bsearch(&tmp_entry, backlog, backlog_len, sizeof(Entry *), compare_entry_ip)) != NULL)
    {
	cur_entry = *found_entry;
	cur_entry->counter++;
		
	/* Check if 'counter' divided by 'time' reach the limit. */
	if ((((cur_entry->counter * NANOSEC_IN_SEC) / (get_time() - cur_entry->timestamp)) ) > max_request)
	{	
	    /* If so, add this ip to blacklist... */
	    blacklist_last = (blacklist_last + 1) % blacklist_len;
	    memcpy(blacklist[blacklist_last], tmp_entry, sizeof(Entry));
	    qsort(blacklist, blacklist_len, sizeof(Entry *), compare_entry_ip);
			
	    pthread_mutex_unlock(&sec_lock);
			
	    /* ... And return. */
	    asprintf(&ip, "IP: %lu", ip_num);
	    log_message(ERROR, EMSG_ADDBLACKLIST, ip);
	    free(tmp_entry);
	    free(ip);
			
	    return (ECOD_ADDBLACKLIST);
	}
	else
	{
	    /* Modify entry to take the new request into consideration. */
	    cur_entry->timestamp = get_time();
	}
    }
    else
    {
	/* Always overwrite the last one in the circular list sorted by date. */
	backlog_last = (backlog_last + 1) % backlog_len;
	memcpy(backlog[backlog_last], tmp_entry, sizeof(Entry));
	qsort(backlog, backlog_len, sizeof(Entry *), compare_entry_ip);
    }
	
    pthread_mutex_unlock(&sec_lock);
	
    free(tmp_entry);
    return (EXIT_SUCCESS);
}

/* close_security()
 * 
 * */
int
close_security			()
{
    int i;

    if (!sec_enabled)
    {
	return (EXIT_SUCCESS);
    }
	
    for (i = 0; i < backlog_len; i++)
    {
	free(backlog[i]);
    }
	
    free(backlog);
	
    for (i = 0; i < blacklist_len; i++)
    {
	free(blacklist[i]);
    }
	
    free(blacklist);
	
    return (EXIT_SUCCESS);
}
