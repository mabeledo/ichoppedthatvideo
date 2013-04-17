/* Stream module.
 * File: stream.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * FLV loading and stream related functions.
 * */

#define _GNU_SOURCE

#include <string.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#include "common.h"
#include "conn.h"
#include "reply.h"
#include "request.h"
#include "stream.h"

/* ********** Constant definitions ********** */
/* Maximum memory percentage available. */
#define MAX_MEM_AVAIL		0.60

#define NANOSEC_IN_SEC		1000000000
#define NEXT_IFRAME		2

/* Time buffer from which the algorithm raises stream quality. */
#define UPPER_LIMIT_TIME	2000000000

/* Time buffer below which the algorithm lowers stream quality. */
#define LOWER_LIMIT_TIME	1500000000

/* Chunk size for chunked transfers. */
#define CHUNK_SIZE		1024
#define CHUNK_END		0

/* File containing information about streams. */
#define	FILE_INFO		"data.txt"

/* Allowed video file extensions. */
#define FLV_EXT			"flv"
#define MP4_EXT			"mp4"

/* Renew commands.
 * These are used to change certain parameters while streaming.
 * */ 
#define RNEW_QUALITY_PARAM_CODE	0
#define RNEW_POS_PARAM_CODE	1

#define RNEW_QUALITY_PARAM_NAME	"quality"
#define RNEW_POS_PARAM_NAME	"pos"

/* Video extensions. */
static const
char * allowed_ext [] = {FLV_EXT, MP4_EXT};

enum allowed_ext_codes {FLV_EXT_CODE, MP4_EXT_CODE};

static const
int allowed_vlen = 2;

/* Connection parameters that can be changed on the fly, or while data
 * is being transmitted to the client.
 * */
const
char * renew_param_names [] = {RNEW_QUALITY_PARAM_NAME, RNEW_POS_PARAM_NAME};

const
int renew_param_vlen = 2;

/* ********** Type definitions ********** */

/* The 'Stream' structure holds all the data about each video file, and
 * each 'Video' structure has at least one 'Stream'.
 * */
typedef
struct _stream
{	
    ushort type;
		
    uint8_t * data;
    int64_t data_size;
	
    /* Average size between iframes */
    int avg_size;
	
    /* Array with all the iframe offsets in the file. */
    int64_t * iframe_offset;
    ushort iframe_num;
}
Stream;

/* The 'Video' structure does not point to an unique video, but a
 * stream set containing several versions, each one with different
 * quality settings.
 * The 'Stream' array is ordered by stream size, from higher to lower.
 * */
typedef
struct _video
{
    /* Duplicate some important Ad fields, in order to locate
     * streams easily.
     * */
    int pos;
	
    int id;
    char * path;
    char * sign;
    int counter;

    Stream ** streams;
    int stream_num;
    int64_t size;
}
Video;

/* ********** Global variables ********** */

/* Default reply parameters for sending streams.
 *  - http_command: no command, just send data.
 *  - http_code: OK code.
 *  - content_type: NULL.
 *  - connection: closed
 *  - transfer_encoding: NULL.
 *  - cache_control: no cache.
 *  - pragma: no cache.
 *  - expiration: no expiration.
 * */
ReplyParams default_params = {NULL, HTTP_OK_CODE, NULL, CONN_CLOSE, NULL,
			      NO_CACHE, NO_EXPIRE};

/* Video related variables. */
char * video_path    = NULL;
Video ** videos      = NULL;
int videos_num       = 0;
int signed_auth      = 0;

/* Thread related variables. */
pthread_mutex_t stream_lock = PTHREAD_MUTEX_INITIALIZER;

/* Connection timeouts. */
int timeout_sec  = 0;

/* Buffer and data sizes to ensure stable transmissions. */
int send_buffer      = 0;
int chunk_size       = 0;

/* Memory usage limits. */
int64_t mem_avail    = 0;
int64_t mem_used     = 0;

/* ********** Private functions ********** */

/* get_next_offset()
 * 
 * Takes an offset position in an array and returns the next one.
 * */
inline
ushort
get_next_offset				(const Stream * cur_stream, const ushort first_offset, ushort jump)
{
    ushort next_offset;
    next_offset = first_offset + jump;
	
    while ((next_offset < cur_stream->iframe_num) &&
	   (cur_stream->iframe_offset[first_offset] == cur_stream->iframe_offset[next_offset]))
    {
	next_offset++;
    }

    return (next_offset);
}

/* compare_video_id()
 * 
 * Function to be used with bsearch() to search for a video.
 * */
inline
int
compare_video_id			(const void * fst, const void * snd)
{
    Video ** fst_video = (Video **) fst;
    Video ** snd_video = (Video **) snd;
	
    /* Take care of NULL pointers. */
    if (*fst_video == NULL)
    {
	return (-1);
    }
	
    if (*snd_video == NULL)
    {
	return (1);
    }

    return ((*fst_video)->id - (*snd_video)->id);
}

/* compare_stream_size()
 * 
 * Function to be used along with qsort() to sort Streams by size.
 * */
inline
int
compare_stream_size			(const void * fst, const void * snd)
{
    Stream ** fst_stream = (Stream **) fst;
    Stream ** snd_stream = (Stream **) snd;

    return ((*fst_stream)->data_size - (*snd_stream)->data_size);
}

/* load_video()
 * 
 * Localize all video files under a specific directory and load them
 * to feed a request.
 * Returns a 'Video' pointer that should be freed after use, only if it
 * is found. Returns NULL otherwise.
 * This function is thread safe.
 * */
Video *
load_video				(char * id, char * sign)
{
    Video * cur_video, * search_video, ** found_video;
    Stream * stream;
    char * filename, * file_data, * offset, * ext, * field, * next_valid;
    int i, j;
	
    /* Check if this directory is supported. */
    if (check_supported_dir(video_path, id) == 0)
    {
	log_message(ERROR, EMSG_STREAMIDUNK, id);
	return (NULL);
    }
	
    /* Is this video already in memory? If so, use that. If not, get it
     * from hard disk.
     * */
    search_video = malloc(sizeof(Video));
    search_video->id = atoi(id);	
    pthread_mutex_lock(&stream_lock);

    /* Search in video cache.
     *  - There is at least one video in memory.
     *  - Video ID is found in cache.
     *  - Video sign matches with the requested one.
     * */ 
    if ((videos_num > 0) &&
	((found_video = bsearch(&search_video, videos, videos_num, sizeof(Video *), compare_video_id)) != NULL) &&
	(!signed_auth || (strcmp((*found_video)->sign, sign) == 0)))
    {
	cur_video = *found_video;
	cur_video->counter++;
	pthread_mutex_unlock(&stream_lock);
	free(search_video);
	return (cur_video);
    }
	
    free(search_video);
	
    /* Load the info file. */
    asprintf(&filename, "%s/%s/%s", video_path, id, FILE_INFO);

    if (!(get_file_size(filename) > 0) ||
	(file_data = (char*)get_file_contents(filename)) == NULL)
    {
	free(filename);
	pthread_mutex_unlock(&stream_lock);
	log_message(ERROR, EMSG_NODATAFILE, filename);
	return (NULL);
    }

    free(filename);
    cur_video = malloc(sizeof(Video));
	
    /* Check video sign. */
    cur_video->id = atoi(id);
    cur_video->path = get_first_substr(file_data, '\n');
    cur_video->sign = get_first_substr(strchr(file_data, '\n') + 1, '\n');
    cur_video->counter = 1;

    if ((signed_auth) &&
	((strlen(sign) != SIGN_LEN) || (strncmp(cur_video->sign, sign, SIGN_LEN) != 0)))
    {
	pthread_mutex_unlock(&stream_lock);
	log_message(WARNING, EMSG_INVALSIGN, cur_video->path);

	free(file_data);
	free(cur_video->sign);
	free(cur_video->path);
	free(cur_video);
	return (NULL);
    }

    offset = strchr(strchr(file_data, '\n') + 1, '\n');

    /* Read the number of available streams. */
    cur_video->stream_num = strtod(offset + 1, &offset);
    cur_video->streams = malloc(cur_video->stream_num * sizeof(Stream *));
	
    /* Load each video file found. */
    i = 0;
    while (i < cur_video->stream_num)
    {
	/* Read the name of the first video file. */
	field = get_between_delim(offset, '\n', '\n');
	asprintf(&filename, "%s/%s", cur_video->path, field);
	free(field);
		
	stream = malloc(sizeof(Stream));
		
	if ((stream->data = get_file_contents(filename)) == NULL)
	{
	    /* This stream does not exist. */
	    free(stream);
	    cur_video->stream_num--;
	    cur_video->streams = realloc(cur_video->streams, cur_video->stream_num * sizeof(Stream *));
	    log_message(WARNING, EMSG_NOSTREAM, filename);
	    free(filename);
	}
	else
	{
	    stream->data_size = get_file_size(filename);

	    /* Find the stream type. 
	     * In the type check, into 'send_video', the switch() checks if
	     * this type is one of the allowed or has an undefined value
	     * (a value bigger than 'allowed_vlen'.
	     * */
	    ext = get_last_substr(filename, '.');
	    for (j = 0; ((j < allowed_vlen) && (strstr(ext, allowed_ext[j]) == NULL)); j++);
	    stream->type = j;
	    free(filename);
	    free(ext);
			
	    /* Read the number of iframes. */
	    offset = strchr(offset + 2, '\n');
	    stream->iframe_num = strtod(offset + 1, &offset);
	    stream->avg_size = stream->data_size / stream->iframe_num;
			
	    /* Allocate memory for the offsets. */
	    stream->iframe_offset = malloc(stream->iframe_num * sizeof(int64_t));
			
	    /* Read each iframe offset. */
	    for (j = 0; j < stream->iframe_num; j++)
	    {
		stream->iframe_offset[j] = strtoll(offset, &next_valid, 10);
		offset = next_valid;
	    }
			
	    /* Check if the current stream iframe offsets are well formed,
	     * i.e., there is no iframe offsets beyond the last byte.
	     * */
	    if (stream->iframe_offset[stream->iframe_num - 1] < stream->data_size)
	    {
		cur_video->streams[i] = stream;
		cur_video->size += stream->data_size;
		i++;
	    }
	    else
	    {
		free(stream->iframe_offset);
		free(stream);
		cur_video->stream_num--;
		cur_video->streams = realloc(cur_video->streams, cur_video->stream_num * sizeof(Stream *));
		log_message(WARNING, EMSG_INVALOFFSET, filename);
	    }
	}
    }

    mem_used += cur_video->size;
    free(file_data);

    if (cur_video->stream_num > 0)
    {
	/* Sort streams array. */
	qsort(cur_video->streams, cur_video->stream_num, sizeof(Stream *), compare_stream_size);
    }
    else
    {
	/* Return an error and free allocated memory. */
	pthread_mutex_unlock(&stream_lock);
	log_message(ERROR, EMSG_NOSTREAMAVAIL, cur_video->path);
		
	free(cur_video->sign);
	free(cur_video->path);
	free(cur_video);
	return (NULL);
    }
	
    /* Add this video to the list and sort it if necessary.
     * This is controlled by the application-wide mutex.
     * */
    videos_num++;
    videos = realloc(videos, videos_num * sizeof(Video *));
    videos[videos_num - 1] = cur_video;
	
    if (videos_num > 1)
    {
	qsort(videos, videos_num, sizeof(Video *), compare_video_id);
    }

    pthread_mutex_unlock(&stream_lock);
    return (cur_video);
}

/* unload_video()
 * 
 * Free memory allocated for a video.
 * This process is here because of the huge amount of code in
 * send_video().
 * This function is thread safe.
 * */
int
unload_video			(Video * video)
{
    int i, counter;

    pthread_mutex_lock(&stream_lock);
	
    /* Delete video from memory if counter reaches 0 and video time alive
     * in memory reaches MAX_TIME_ALIVE.
     * */
    if ((counter = --video->counter) <= 0)
    {
	/* Free allocated streams. */
	for (i = 0; i < video->stream_num; i++)
	{
	    free(video->streams[i]->data);
	    free(video->streams[i]->iframe_offset);
	    free(video->streams[i]);
	}

	free(video->streams);
	free(video->path);
	free(video->sign);
	mem_used -= video->size;
	video->id = INT_MAX;
	qsort(videos, videos_num, sizeof(Video *), compare_video_id);
	free(video);
	videos_num--;
		
	/* Reallocating this vector might fail. If so, initialize
	 * 'videos_num' to 0
	 * */
	if ((videos = realloc(videos, videos_num * sizeof(Video *))) == NULL)
	{
	    videos_num = 0;
	}
    }

    pthread_mutex_unlock(&stream_lock);
    return (counter);	
}

/* ********** Public functions ********** */

/* init_videos()
 * 
 * Initialize streaming and load a default video.
 * */
int
init_videos			(char * path, int auth, int timeout)
{
    struct sysinfo info;
	
    /* Determine the amount of memory available. This is not used right
     * now, but might be an interesting option in the future.
     * */
    if (sysinfo(&info) != 0)
    {
	log_message(CRITICAL, EMSG_FREEMEM, NULL);
	return (ECOD_FREEMEM);
    }
	
    mem_avail = info.freeram * MAX_MEM_AVAIL;
	
    if (!check_file_exists(path) ||
	(asprintf(&video_path, "%s", path) <= 0))
    {
	log_message(CRITICAL, EMSG_VIDEOPATH, NULL);
	return (ECOD_VIDEOPATH);
    }

    /* Enable signed video requests. */
    signed_auth = auth;

    /* Timeout for send(). */
    if (timeout > MIN_TIMEOUT)
    {
	timeout_sec = timeout;
    }
    else
    {
	timeout_sec = MIN_TIMEOUT;
	log_message(MESSAGE, IMSG_INVALTIMEOUT, NULL);
    }

    return (EXIT_SUCCESS);
}

/* send_video()
 *
 * The function receives a socket descriptor and the request parameters,
 * and returns the amount of data written.
 * */
int
send_video			(int client_sd, char ** params)
{
    /* Reply parameters. */
    ReplyParams send_params;
	
    /* Needed to change sending options on the fly. */
    char ** renew_params;
    const int renew_params_buf_len = 512 * sizeof(char);

    /* Sending data. */
    struct timespec start_time, stop_time;
    int spent_time, cached_time;
    ushort cur_stream_pos, first_iframe, next_iframe, temp_iframe;
    int total_bytes_sent, bytes_sent, chunks_sent, chunks_to_send;
	
    /* Video structures. */
    Video * cur_video;
    Stream * cur_stream;

    /* 'output_buf' is the buffer used to send data over the net.
     * 'data_buf' is used to concatenate stream data.
     * 'output_buf_len_str' contains the length of the 'output_buf' in char * format.
     * 'data_buf_len_str' is the same as above for 'data_buf'.
     * 'output_buf_len' and 'data_buf_len' are the length of the previous buffers.
     * 'output_buf_len_len' and 'data_buf_len_len' are the length of the strings
     * containing the length of the buffers (little messy).
     * 'chunk_len' is the size of the data chunk actually sent.
     * */
    char * output_buf, * output_buf_len_str, * data_buf_len_str;
    uint8_t * data_buf;
    unsigned int output_buf_len, data_buf_len, output_buf_len_len, data_buf_len_len, chunk_len;
	
    /* Socket parameters. 
     * 'send_buffer', is a memory segment assigned to this socket to perform better sending
     * larger chunks.
     * 'keepalive', if keepalive option is on or off.
     * 'keepalive_time', time (in seconds) between the last data packet sent and the first keepalive probe.
     * 'keepalive_intvl', time (in seconds) between probes.
     * 'keepalive_probes', number of probes.
     * 'send_timeout', is a timeout (in seconds) for send()
     */
    const int send_buffer = 524288;
    const int keepalive = 1;                    /* TODO: This four variables may be global. */
    const int keepalive_probes = 10;
    const int keepalive_intvl = 5;
    const int keepalive_time = timeout_sec - (keepalive_probes * keepalive_intvl);
    struct timeval send_timeout;

    /* Initialization.
     * */
    if ((params[VIDEOID_PARAM_CODE] != NULL) &&
	(!signed_auth || (params[SIGN_PARAM_CODE] != NULL)))
    {
	/* Load the video selected.
	 * It checks if the provided video id is available, and if the
	 * video sign is valid.
	 * */
	if ((cur_video = load_video(params[VIDEOID_PARAM_CODE], params[SIGN_PARAM_CODE])) == NULL)
	{
	    log_message(WARNING, EMSG_NOVIDEO, params[VIDEOID_PARAM_CODE]);
	    return (ECOD_NOVIDEO);
	}
    }
    else
    {
	/* There is no video id or sign. */
	log_message(WARNING, EMSG_INVALVIDEO, params[VIDEOID_PARAM_CODE]);
	return (ECOD_INVALVIDEO);
    }
	
    /* Select the quality specified on URL parameters, if there is one.
     * Check also if the quality code is between limits.
     * (Warren 2003, pg. 51)
     * */
    if (!(params[QUALITY_PARAM_CODE] &&
	  ((cur_stream_pos = (ushort)atoi(params[QUALITY_PARAM_CODE])) <= (cur_video->stream_num - 1))))
    {
	cur_stream_pos = (ushort)floor(cur_video->stream_num / 2);
    }
    else
    {
	log_message(MESSAGE, IMSG_QUALITYSEL, params[QUALITY_PARAM_CODE]);
    }
	
    cur_stream = cur_video->streams[cur_stream_pos];

    /* First iframe could be either defined on URL, or '0' if the stream
     * should be played from the start. If the iframe specified is beyond
     * limits (higher than 'iframe_num' or lower than '0'), set it to '0'.
     * */
    if (!(params[POS_PARAM_CODE] &&
	  ((first_iframe = (ushort)atoi(params[POS_PARAM_CODE])) <= (cur_stream->iframe_num - 1))))
    {
	first_iframe = 0;
    }
    else
    {
	log_message(MESSAGE, IMSG_POSITIONSEL, params[POS_PARAM_CODE]);
    }

    /* Compose a reply according to the file type. */
    send_params = default_params;
	
    /* Cache control. */
    if (params[CACHE_PARAM_CODE] != NULL)
    {
	asprintf(&send_params.cache_control, "%s%s", MAX_AGE, params[CACHE_PARAM_CODE]);
    }

    switch (cur_stream->type)
    {
	case (FLV_EXT_CODE):
	    send_params.content_type = FLV_TYPE;
	    break;
	case (MP4_EXT_CODE):
	    send_params.content_type = MP4_TYPE;
	    break;
	default:
	    send_params.content_type = PLAIN_TYPE;
	    break;
    }

    /* Enlarge the default send() buffer to perform better. */
    setsockopt(client_sd, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(int));

    /* Set keepalive.
     * Eliminar? Es redundante?
    
    setsockopt(client_sd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(int));
    setsockopt(client_sd, SOL_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(int));
    setsockopt(client_sd, SOL_TCP, TCP_KEEPCNT, &keepalive_probes, sizeof(int));
    setsockopt(client_sd, SOL_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(int)); */

    /* Set a default send() timeout at first. 
     * This can be DANGEROUS, and in a near future should be removed.
     * */
    send_timeout.tv_sec = DEFAULT_TIMEOUT + timeout_sec;
    send_timeout.tv_usec = 0;
    setsockopt(client_sd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(struct timeval));

    spent_time = 0;
    total_bytes_sent = 0;
    chunk_len = CHUNK_SIZE;

    /* Program must select between two different behaviours:
     *  - Send a whole file if there is only one video in the directory or
     *    a precise video quality is selected.
     *  - Use an adaptative algorithm to switch between video streams on
     *    the fly.
     * First approach should perform better, as it is simpler and delivers
     * the entire stream at once, allowing the kernel take care of it.
     * */
    if ((cur_video->stream_num == 1) || (params[QUALITY_PARAM_CODE]))
    {
	/* There are some code duplication. Do not blame on me, as it is
	 * not my idea.
	 * */
	data_buf_len = cur_stream->data_size - cur_stream->iframe_offset[first_iframe];
	data_buf = cur_stream->data + cur_stream->iframe_offset[first_iframe];
	output_buf = compose_reply(send_params, data_buf, data_buf_len, &output_buf_len);

	if (params[CACHE_PARAM_CODE] != NULL)
	{
	    free(send_params.cache_control);
	}

	chunks_to_send = (int)ceil((double)output_buf_len / chunk_len);
	chunks_sent = 0;
		
	do
	{
	    clock_gettime(CLOCK_REALTIME, &start_time);	

	    if ((bytes_sent = send(client_sd,
				   output_buf + (chunks_sent * chunk_len),
				   chunk_len - ((chunks_sent + 1) == chunks_to_send) * ((chunks_to_send * chunk_len) - output_buf_len),
				   MSG_NOSIGNAL)) > 0)
	    {
		total_bytes_sent += bytes_sent;
		chunks_sent++;
	    }

	    clock_gettime(CLOCK_REALTIME, &stop_time);
	    spent_time += (int)ceil((double)(((stop_time.tv_sec * NANOSEC_IN_SEC) + stop_time.tv_nsec) - 
					     ((start_time.tv_sec * NANOSEC_IN_SEC) + start_time.tv_nsec)) / NANOSEC_IN_SEC);


	    /* Set a send() timeout. 
	     * This can be DANGEROUS, and in a near future should be removed.
	     * */
	    send_timeout.tv_sec = ceil((spent_time / total_bytes_sent) * chunk_len) + timeout_sec;
	    send_timeout.tv_usec = 0;
	    setsockopt(client_sd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(struct timeval));
	}
	while ((chunks_sent < chunks_to_send) && (bytes_sent > 0));

	if (bytes_sent < 0)
	{
	    log_message(MESSAGE, IMSG_VIDEOSTOP, cur_video->path);
	}
		
	free(output_buf);
	unload_video(cur_video);

	return (total_bytes_sent);
    }
    else
    {
	/* Compose a 'chunked' reply and send the header and the first chunk. */
	send_params.transfer_encoding = CHUNKED;
	next_iframe = get_next_offset(cur_stream, first_iframe, NEXT_IFRAME);
	data_buf_len = cur_stream->iframe_offset[next_iframe] - cur_stream->iframe_offset[first_iframe];
	next_iframe = get_next_offset(cur_stream, next_iframe, 1);
		
	data_buf_len_len = asprintf(&data_buf_len_str, "%x%s", data_buf_len, CRLF);
	data_buf = malloc(data_buf_len + data_buf_len_len);
	memcpy(data_buf, data_buf_len_str, data_buf_len_len);
	memcpy(data_buf + data_buf_len_len, cur_stream->data + cur_stream->iframe_offset[first_iframe], data_buf_len);
		
	output_buf = compose_reply(send_params, data_buf, data_buf_len + data_buf_len_len, &output_buf_len);
	free(data_buf);
	free(data_buf_len_str);

	if (params[CACHE_PARAM_CODE] != NULL)
	{
	    free(send_params.cache_control);
	}

	if ((bytes_sent = send(client_sd, output_buf, output_buf_len, MSG_NOSIGNAL)) < 0)
	{
	    log_message(MESSAGE, IMSG_VIDEOSTOP, cur_video->path);
	    free(output_buf);
	    unload_video(cur_video);
			
	    return (total_bytes_sent);
	}

	free(output_buf);
	cached_time = 0;
	total_bytes_sent += bytes_sent;

        /* Recalculate send() timeout using SO_SNDTIMEO.
	 * This can be DANGEROUS, and in a near future should be removed.
	 * */
	send_timeout.tv_sec = ceil((spent_time / total_bytes_sent) * chunk_len) + timeout_sec;
	send_timeout.tv_usec = 0;
	setsockopt(client_sd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(struct timeval));
    }

    /* Function did not returned, so there is data left to send.
     * Main loop to dispatch data. It sends ~1 second or 1 i-frame
     * per iteration. 
     * */
    while (next_iframe <= cur_stream->iframe_num)
    {
	/* Read data between the next two iframes. */
	data_buf_len = (next_iframe == cur_stream->iframe_num) ? 
	    (cur_stream->data_size - cur_stream->iframe_offset[next_iframe - 1]) : cur_stream->iframe_offset[next_iframe] - cur_stream->iframe_offset[next_iframe - 1];
	data_buf = malloc(data_buf_len);
	memcpy(data_buf,  cur_stream->data + cur_stream->iframe_offset[next_iframe - 1], data_buf_len);
	chunks_to_send = (int)ceil((double)data_buf_len / chunk_len);
	chunks_sent = 0;
		
	/* Send data in chunks. */
	while (chunks_sent < chunks_to_send)
	{
	    /* Take care of the last chunk, so it may be smaller than
	     * CHUNK_SIZE bytes
	     * */
	    output_buf_len = (chunks_sent != (chunks_to_send - 1)) ? chunk_len : (data_buf_len - (chunks_to_send - 1) * chunk_len);
	    output_buf_len_len = asprintf(&output_buf_len_str, "%s%x%s", CRLF, output_buf_len, CRLF);
	    output_buf = malloc((output_buf_len + output_buf_len_len) * sizeof(char));

	    memcpy(output_buf, output_buf_len_str, output_buf_len_len);
	    memcpy(output_buf + output_buf_len_len, data_buf + (chunks_sent) * chunk_len, output_buf_len);
	    output_buf_len += output_buf_len_len;
	    
            /* Start counting time... */
	    clock_gettime(CLOCK_REALTIME, &start_time);

	    /* Send a chunk and check if the operation is done. */
	    if ((bytes_sent = send(client_sd, output_buf, output_buf_len, MSG_NOSIGNAL)) < 0)
	    {
		log_message(MESSAGE, IMSG_VIDEOSTOP, cur_video->path);
		free(data_buf);
		free(output_buf);
		free(output_buf_len_str);
		unload_video(cur_video);
	
		return (total_bytes_sent);
	    }

	    /* Calculate time spent sending the buffer. */
	    clock_gettime(CLOCK_REALTIME, &stop_time);
	    spent_time += (int)ceil((double)(((stop_time.tv_sec * NANOSEC_IN_SEC) + stop_time.tv_nsec) - 
					 ((start_time.tv_sec * NANOSEC_IN_SEC) + start_time.tv_nsec)) / NANOSEC_IN_SEC);
	    
	    free(output_buf);
	    free(output_buf_len_str);
	    total_bytes_sent += bytes_sent;
	    chunks_sent++;

	    /* Recalculate send() timeout using SO_SNDTIMEO.
	     * This can be DANGEROUS, and in a near future should be removed.
	     * */
	    send_timeout.tv_sec = ceil((spent_time / total_bytes_sent) * chunk_len) + timeout_sec;
	    send_timeout.tv_usec = 0;
	    setsockopt(client_sd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(struct timeval));
	}
		
	free(data_buf);

	/* TODO: Change this!
	 * Assuming that it sends ALWAYS 1 second of video.
	 * */
	if ((cached_time + (1 - spent_time)) > 0)
	{
	    cached_time += 1 - spent_time;
	}

	/* Adaptive streaming.
	 * 
	 * If 'cached_time' is lower than 'MIN_SEND_TIME', change stream
	 * to a higher resolution.
	 * */
	if (next_iframe <= cur_stream->iframe_num)
	{
	    if ((cached_time > UPPER_LIMIT_TIME) && (cur_stream_pos < (cur_video->stream_num - 1)))
	    {
		cur_stream_pos++;
		cur_stream = cur_video->streams[cur_stream_pos];
		log_message(MESSAGE, IMSG_BITRATEHIGH, cur_video->path);
	    }
	    else
	    {
		/* If 'cached_time' is greater than 'MAX_SEND_TIME',
		 * change stream to a lower resolution.
		 * */
		if ((cached_time < LOWER_LIMIT_TIME) && (cur_stream_pos > 0))
		{
		    cur_stream_pos--;
		    cur_stream = cur_video->streams[cur_stream_pos];
		    log_message(MESSAGE, IMSG_BITRATELOW, cur_video->path);
		}
	    }
	}
		
	next_iframe = get_next_offset(cur_stream, next_iframe, 1);
		
	/* Read status data from client. Do not wait if there is no data
	 * available.
	 * TODO
	 * */
	data_buf = malloc(renew_params_buf_len);
		
	if (recv(client_sd, data_buf, renew_params_buf_len, MSG_DONTWAIT) > 0)
	{
	    log_message(MESSAGE, IMSG_CLIENTMSG, (char *)data_buf);			
	    renew_params = parse_key_value((char * )data_buf, renew_param_names, renew_param_vlen, "=&", PARAM_MAX_LEN);
			
	    /* Select stream quality. */
	    if (renew_params[RNEW_QUALITY_PARAM_CODE])
	    {
		cur_stream_pos = atoi(renew_params[RNEW_QUALITY_PARAM_CODE]);
		free(renew_params[RNEW_QUALITY_PARAM_CODE]);
				
		if (cur_stream_pos >= cur_video->stream_num)
		{
		    cur_stream_pos = cur_video->stream_num - 1;
		}
		else
		{
		    if (cur_stream_pos < 0)
		    {
			cur_stream_pos = 0;
		    }
		}
	    }
			
	    /* Select position into stream. */
	    if (renew_params[RNEW_POS_PARAM_CODE])
	    {
		temp_iframe = atoi(renew_params[RNEW_POS_PARAM_CODE]);
		free(renew_params[RNEW_POS_PARAM_CODE]);
				
		if ((temp_iframe < cur_stream->iframe_num) &&
		    (temp_iframe > 0))
		{
		    next_iframe = temp_iframe;
		}
	    }
			
	    if (renew_params)
	    {
		free(renew_params);
	    }
	}
		
	free(data_buf);
    }
	
    /* Send ending chunk. */
    output_buf_len = asprintf(&output_buf, "%s%x%s%s", CRLF, CHUNK_END, CRLF, CRLF);

    if ((bytes_sent = send(client_sd, output_buf, output_buf_len, MSG_NOSIGNAL)) < 0)
    {
	log_message(MESSAGE, IMSG_VIDEOSTOP, cur_video->path);
	free(output_buf);
	unload_video(cur_video);
		
	return (total_bytes_sent);
    }
	
    free(output_buf);
    total_bytes_sent += bytes_sent;
    unload_video(cur_video);
	
    return (total_bytes_sent);
}

/* close_videos()
 * 
 * Free all memory allocated in this module.
 * */
int
close_videos				()
{
    int i;

    if (videos_num > 0)
    {
	i = videos_num - 1;
	while (i < videos_num)
	{
	    if (unload_video(videos[i]) <= 0)
	    {
		i++;
	    }
	}
		
	free(videos);
    }
	
    return (EXIT_SUCCESS);
}
