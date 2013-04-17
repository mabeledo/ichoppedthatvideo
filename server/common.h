/* Common module.
 * File: common.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 *
 * Define usual data and utilities.
 * */

#ifndef COMMON_H
#define COMMON_H

#define FALSE			0
#define TRUE			!FALSE

#define CRLF			"\r\n"
#define CRLF_LEN		3

#define SIGN_LEN		40
#define PARAM_MAX_LEN	64

typedef short			Boolean;

/* ********** Common includes ********** */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "msg.h"
#include "logging.h"

/* ********** Public functions ********** */

/* File related functions */
int
check_file_exists		(char * filename);

unsigned int
get_file_size			(char * filename);

uint8_t *
get_file_contents		(char * filename);

int
check_supported_dir		(char * path, char * id);

/* String related functions */
char *
get_between_delim		(const char * str, char delim_head, char delim_tail);

char *
get_first_substr		(const char * str, char delim);

char *
get_last_substr			(const char * str, char delim);

size_t
get_valid_substr_len	        (const char * str, char * valid_set);

char *
concat_and_free			(char * fst, char * snd);

void
find_and_replace		(char ** src, const char * old, const char * new);

short
find_vector_first		(char * str, const char ** vector, size_t len);

char **
parse_key_value			(char * str, const char ** keys, int len, char * delimiters, int max_len);

char *
wipe_special_chars		(char * str, int len);

/* Atomic functions. */
inline
void
atomic_add_int			(int * num);

inline
void
atomic_sub_int			(int * num);

inline
void
atomic_set_int			(int * num, int value);

/* Search related functions. */

int
comp_int64_t			(const void * fst, const void * snd);

/* System functions */

int64_t
get_time			();

const char *
get_server_name			();

#endif
