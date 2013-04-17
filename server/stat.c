/* Statistics module.
 * File: stat.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * */

#define _GNU_SOURCE

#include <pthread.h>
#include <mysql.h>
#include <GeoIP.h>
#include <GeoIPCity.h>
#include <openssl/sha.h>

#include "common.h"
#include "conn.h"
#include "stat.h"
 
/* ********** Constant definitions ********** */
#define DEFAULT_PATH	    "/etc/ichoppedthatvideo/passwd"
#define DATABASE	    "ichoppedthatvideo"
#define REQUEST_FIELDS	    "client_id, server_id, child_id, type, browser, opsys, city, country, cur_timestamp"
#define STREAM_FIELDS	    "request_id, video_id, seconds"
#define UNKNOWN_FIELD	    "unknown"

/* ********** Global variables ********** */
int stats_enabled         = 0;
pthread_mutex_t stat_lock = PTHREAD_MUTEX_INITIALIZER;
MYSQL * db_conn           = NULL;
GeoIP * gi_db             = NULL;

/* ********** Public functions ********** */

/* init_stat()
 * 
 * Initialize MySQL and GeoIP API.
 * Needs a 'path' to a passwd file. If it is NULL or does not exist,
 * will use the 'DEFAULT_PATH' constant defined previously.
 * However, 'host' can not be NULL.
 * */
int
init_stat				(char * host, char * path)
{
    char * contents, * entry, * user, * passwd, * add_info;
	
    if (host == NULL)
    {
	log_message(ERROR, EMSG_DBHOST, NULL);
	return (ECOD_DBHOST);
    }
	
    /* Find required login and password.
     * If 'path' is NULL or does not exist, use a default value.
     * */
    if ((path == NULL) || (!check_file_exists(path)))
    {
	contents = (char *)get_file_contents(DEFAULT_PATH);
    }
    else
    {
	contents = (char *)get_file_contents(path);
    }
	
    entry = strstr(contents, DATABASE);
    entry = strchr(entry, '=') + 1;
	
    if ((user = get_first_substr(entry, ',')) == NULL)
    {
	free(contents);
	log_message(ERROR, EMSG_STATDBUSER, NULL);
	return (ECOD_STATDBUSER);
    }
	
    if ((passwd = get_between_delim(entry, ',', ';')) == NULL)
    {
	free(user);
	free(contents);
	log_message(ERROR, EMSG_STATDBPASS, NULL);
	return (ECOD_STATDBPASS);
    }
    free(contents);

    /* Connect to MySQL database. */
    if (((db_conn = mysql_init(NULL)) == NULL) ||
	(mysql_real_connect(db_conn, host, user, passwd, DATABASE, 0, NULL, 0) == NULL))
    {
	free(user);
	free(passwd);
	mysql_close(db_conn);
	log_message(ERROR, EMSG_DBSTATCONN, NULL);
	return (ECOD_DBSTATCONN);
    }
	
    free(user);
    free(passwd);
	
    /* GeoIP, only for request statistics.
     * Initialization is done here to avoid performance and memory usage
     * problems while analyzing request data.
     * */
    if ((gi_db = GeoIP_open("/usr/share/GeoIP/GeoLiteCity.dat", GEOIP_STANDARD)) == NULL)
    {
	log_message(ERROR, EMSG_GISTATINIT, NULL);
	return (ECOD_GISTATINIT);
    }
	
    asprintf(&add_info, "Server: %s\n", host);
    log_message(MESSAGE, IMSG_STATSENABLED, add_info);
    free(add_info);
    stats_enabled = 1;
    return (EXIT_SUCCESS);
}

/* new_request_stat()
 *
 * Register statistics about client requests. Returns the newly generated
 * and unique id.
 * */
int
new_request_stat			(unsigned long ip_num, const char * type, char * user_agent)
{
    int i, auto_id, client_id_len, child_id;
    char * query, * server_id, * client_id, * tmp_id;
    unsigned char * uchar_sign;
    GeoIPRecord * record;
    MYSQL_RES * result;
    MYSQL_ROW row;
    char * add_info;
	
    /* TODO: browser and opsys detection. */
    const char browser [] = UNKNOWN_FIELD;
    const char opsys [] = UNKNOWN_FIELD;
	
    /* Check if statistic data gathering is enabled. */
    if (stats_enabled != 1)
    {
	return (EXIT_SUCCESS);
    }

    child_id = get_child_pos();
    server_id = (char *)get_server_name();
	
    /* Calculate unique digest. */
    uchar_sign = malloc(SHA_DIGEST_LENGTH);
    client_id = malloc(SIGN_LEN + 1);
    client_id_len = asprintf(&tmp_id, "%lu", ip_num);
    SHA1((unsigned char *)tmp_id, client_id_len, uchar_sign);
    free(tmp_id);
	
    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
	sprintf(client_id + i * 2, "%02x", uchar_sign[i]);
    }

    free(uchar_sign);
	
    if ((record = GeoIP_record_by_ipnum(gi_db, ip_num)) != NULL)
    {
	asprintf(&query, "INSERT INTO requests (%s) VALUES ('%s', '%s', %d, '%s', '%s', '%s', '%s', '%s', CURRENT_TIMESTAMP())",
		 REQUEST_FIELDS, client_id, server_id, child_id, type, browser, opsys, record->city, record->country_name);
	GeoIPRecord_delete(record);
    }
    else
    {
	asprintf(&query, "INSERT INTO requests (%s) VALUES ('%s', '%s', %d, '%s', '%s', '%s', '%s', '%s', CURRENT_TIMESTAMP())",
		 REQUEST_FIELDS, client_id, server_id, child_id, type, browser, opsys, UNKNOWN_FIELD, UNKNOWN_FIELD);
    }
	
    /* MySQL threaded interaction.
     * Multithread clients sharing the same connection *must* enclose
     * mysql_query() and mysql_store_result() in a lock to prevent access
     * from other threads.
     * */
	
    /* Insert data and get the last autoincremented value. */
    pthread_mutex_lock(&stat_lock);
    if (mysql_query(db_conn, query) != 0)
    {
	pthread_mutex_unlock(&stat_lock);
	log_message(ERROR, EMSG_INSERT, query);
	free(client_id);
	free(server_id);
	free(query);
	return (ECOD_INSERT);
    }

    pthread_mutex_unlock(&stat_lock);
    free(client_id);
    free(query);
    asprintf(&query, "SELECT MAX(id) FROM requests WHERE server_id = '%s' AND child_id = %d", server_id, child_id);
    free(server_id);
	
    pthread_mutex_lock(&stat_lock);
    if (mysql_query(db_conn, query) != 0)
    {
	pthread_mutex_unlock(&stat_lock);
	log_message(ERROR, EMSG_SELECT, query);
	free(query);
	return (ECOD_SELECT);
    }
	
    if ((result = mysql_store_result(db_conn)) != NULL)
    {
	pthread_mutex_unlock(&stat_lock);
	free(query);
	row = mysql_fetch_row(result);
	auto_id = atoi(row[0]);
	mysql_free_result(result);
	asprintf(&add_info, "Request ID: %d\n", auto_id);
	log_message(MESSAGE, IMSG_NEWREQSTAT, add_info);
	free(add_info);
    }
    else
    {
	pthread_mutex_unlock(&stat_lock);
	log_message(ERROR, EMSG_SELECT, query);
	free(query);
	return (ECOD_SELECT);
    }

    return (auto_id);
}

/* new_stream_stat()
 * 
 * Register statistics about streams and its duration.
 * */
int
new_stream_stat			(int request_id, int video_id, int bytes)
{
    char * query, * add_info;
	
    /* Check if statistic data gathering is enabled. */
    if (stats_enabled != 1)
    {
	return (EXIT_SUCCESS);
    }
	
    asprintf(&query, "INSERT INTO streams (%s) VALUES (%d, %d, %d)",
	     STREAM_FIELDS, request_id, video_id, bytes);
	
    pthread_mutex_lock(&stat_lock);		 
    if (mysql_query(db_conn, query) != 0)
    {
	pthread_mutex_unlock(&stat_lock);
	log_message(ERROR, EMSG_INSERT, query);
	free(query);
	return (ECOD_INSERT);
    }		 

    pthread_mutex_unlock(&stat_lock);
    free(query);
    asprintf(&add_info, "Request ID: %d\n", request_id);
    log_message(MESSAGE, IMSG_NEWSTRMSTAT, add_info);
    free(add_info);
    return (EXIT_SUCCESS);
}

/* close_stat()
 * 
 * */
int
close_stat				()
{
    /* Check if statistic data gathering is enabled. */
    if (stats_enabled != 1)
    {
	return (EXIT_SUCCESS);
    }
	
    mysql_close(db_conn);
    GeoIP_delete(gi_db);
	
    log_message(MESSAGE, IMSG_STATCLOSED, NULL);
    return (EXIT_SUCCESS);
}
