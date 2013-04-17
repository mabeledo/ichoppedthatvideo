/* Video module.
 * File: video.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * FLV loading and stream related functions.
 * */ 

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <openssl/sha.h>

#include "../server/common.h"
#include "file.h"
#include "video.h"

/* ********** Constant definitions ********** */
#define FLV_EXT			".flv"
#define MP4_EXT			".mp4"
#define M4V_EXT			".m4v"
#define MOV_EXT			".mov"
#define OGV_EXT			".ogv"
#define WEBM_EXT                ".webm"

/* ********** Type definitions ********** */

struct _stream
{
	char * filename;
	int data_size;
	
	/* Array with all the iframe offsets in the file. */
	int64_t * iframe_offset;
	int iframe_num;
};

typedef struct _stream Stream;

/* ********** Global variables ********** */

/* Video extensions. */
static const
char * allowed_ext [] = {FLV_EXT, MP4_EXT, M4V_EXT, MOV_EXT, OGV_EXT, WEBM_EXT};

static const
size_t allowed_vlen = 6;


/* ********** Private functions ********** */

/* compare_stream_size()
 * 
 * Function to be used along with qsort() to sort Streams by size.
 * */
int
compare_stream_size			(const void * fst, const void * snd)
{
    return (((Stream *)fst)->data_size - ((Stream *)snd)->data_size);
}

/* check_supported_ext()
 * 
 * Check if a struct dirent * belongs to a video supported by the server.
 * This function is intended to use along with scandir().
 * */
int
check_supported_ext			(const struct dirent * file)
{
    int i;
	
    /* This loop search for allowed extensions in the files.
     * It does not match very well, as it only search for the extension
     * but does not take into account the position, so if it finds
     * the extension string into the file name, it will return a 'true'
     * value.
     * */
    for (i = 0; (i < allowed_vlen) && (strstr(file->d_name, allowed_ext[i]) == 0); i++);

    return (i < allowed_vlen);
}

/* load_stream()
 * 
 * Return a 'Stream' structure using avcodec tools.
 * */
Stream *
load_stream			(char * filename)
{
    int stream_index, full_frame;
    Stream * res;
    int64_t prev_offset;
	
    /* AVCodec related types. */
    AVFormatContext * format_ctx;
    AVCodecContext * codec_ctx;
    AVCodec * codec;
    AVPacket pkt;
    AVFrame * frame;

    /* Open the file and get stream information. Errors here are fatal.
     * */
    if ((av_open_input_file(&format_ctx, filename, NULL, 0, NULL) == 0) &&
	(av_find_stream_info(format_ctx) >= 0))
    {
	/* Search for the codec information.
	 * This 'for' sentence is a little messy, but works flawlessly.
	 * */
	for (stream_index = 0;
	     (stream_index < format_ctx->nb_streams) &&
		 (format_ctx->streams[stream_index]->codec->codec_type != CODEC_TYPE_VIDEO);
	     stream_index++);

	if (stream_index >= format_ctx->nb_streams)
	{
	    fprintf(stderr, "Bad file format: %s\n", filename);
	    return (NULL);
	}
		
	codec_ctx = format_ctx->streams[stream_index]->codec;

	/* Locate and open the right codec to manage the stream. */
	if ((codec = avcodec_find_decoder(codec_ctx->codec_id)) == NULL)
	{
	    fprintf(stderr, "Codec not found: %s\n", filename);
	    return (NULL);
	}

	if (avcodec_open(codec_ctx, codec) < 0)
	{
	    fprintf(stderr, "Cannot open code: %s\n", filename);
	    return (NULL);
	}
		
	/* Allocate memory for ~1 i-frames per second and a frame to find them. */
	frame = avcodec_alloc_frame();
	res = malloc(sizeof(Stream));
	res->iframe_offset = malloc((format_ctx->duration / AV_TIME_BASE) * sizeof(int64_t) * 4);
	res->iframe_num = 0;
	prev_offset = 0;
		
	/* Read a raw packet from the container. */
	while (av_read_frame(format_ctx, &pkt) == 0)
	{
	    /* Determine if this packet belongs to the video stream.
	     * If it is, determine if it is a Iblock.
	     * */
	    if (pkt.stream_index == stream_index)
	    {	
		if (avcodec_decode_video(codec_ctx, frame, &full_frame, pkt.data, pkt.size) <= 0)
		{
		    avcodec_close(codec_ctx);
		    av_close_input_file(format_ctx);
					
		    fprintf(stderr, "\nCannot decode video: %s\n", filename);
		    return (NULL);
		}
				
		if (full_frame && (frame->pict_type != FF_I_TYPE))
		{
		    prev_offset = url_ftell(format_ctx->pb);
		}
				
		/* Packet contains a complete frame and that frame is a
		 * key frame, so kept it in the key frame register.
		 * */
		if (full_frame && (frame->pict_type == FF_I_TYPE))
		{
		    res->iframe_offset[res->iframe_num] = prev_offset;
		    res->iframe_num++;
		}
	    }
			
	    av_free_packet(&pkt);
	}

	res->data_size = get_file_size(filename);
	res->filename = get_last_substr(filename, '/');
		
	/* Deallocate memory. */
	avcodec_close(codec_ctx);
	av_close_input_file(format_ctx);
    }
    else
    {
	fprintf(stderr, "Cannot open file: %s\n", filename);
	return (NULL);
    }
	
    return (res);
}

/* ********** Public functions ********** */

/* load_videos()
 * 
 * Localize all the video files in a directory and load them.
 * */
int
load_videos			(char * path)
{
    Stream ** streams;
    struct dirent ** video_files;
    int i, total_size, num_entries, base_len;
    char * filename, * base, * char_sign;
    unsigned char * uchar_sign;
    const time_t timer = time(NULL);
	
    if ((num_entries = scandir(path, &video_files, check_supported_ext, alphasort)) < 1)
    {
	/* No error here. There are directories without video files. */
	return (TRUE);
    }

    printf("Loading video from %s... ", path);
    fflush(stdout);

    streams = malloc(num_entries * sizeof(Stream *));
    av_register_all();
    total_size = 0;
	
    /* Load each file found. Exit if num_entries reach 0 (files found but
     * bad file format).
     * */
    for (i = 0; (i < num_entries) && (num_entries > 0); i++)
    {
	asprintf(&filename, "%s/%s", path, video_files[i]->d_name);
	free(video_files[i]);

	if ((streams[i] = load_stream(filename)) == NULL)
	{
	    /* One stream less... */
	    num_entries--;
	    streams = realloc(streams, num_entries * sizeof(Stream));
	}

	total_size += get_file_size(filename);
	free(filename);
    }
	
    free(video_files);

    /* Calculate unique digest. */
    uchar_sign = malloc(SHA_DIGEST_LENGTH);
    char_sign = malloc(SIGN_LEN + 1);
    base_len = asprintf(&base, "%s%lu%d", path, (unsigned long)timer, total_size);
    SHA1((unsigned char *)base, base_len, uchar_sign);
    free(base);
	
    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
	sprintf(char_sign + i * 2, "%02x", uchar_sign[i]);
    }

    free(uchar_sign);
	
    /* There is at least two video files... */
    if (num_entries > 1)
    {
	/* Sort streams array. */
	qsort(streams, num_entries, sizeof(Stream *), compare_stream_size);
    }

    /* Write results to a file, with this format:
     * - Number of files (streams).
     * - Information about that files:
     *   - File name.
     *   - Number of iframes.
     *   - Iframe offset array.
     * */
    if (!save_common_info(path, char_sign, num_entries))
    {
	return (FALSE);
    }
	
    free(char_sign);
	
    for (i = 0; i < num_entries; i++)
    {
	if (!save_stream_info(path, streams[i]->filename, streams[i]->iframe_offset, streams[i]->iframe_num))
	{
	    free(streams[i]);
	    return (FALSE);
	}
		
	free(streams[i]);
    }

    printf("Done\n");
    fflush(stdout);
	
    free(streams);
	
    return (TRUE);
}

