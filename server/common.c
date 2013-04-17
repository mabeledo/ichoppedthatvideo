/* Common module.
 * File: common.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Define usual data and utilities.
 * */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "common.h"

/* *********** Constant definitions ********** */
#define MAXNUMPATH		999999999
#define MINNUMPATH		100


/* *********** Public functions ********** */

/* File functions
 * File related functions to manipulate, analyze or check file data and
 * properties.
 * */

/* check_file_exists()
 * 
 * */
int
check_file_exists		(char * filename)
{
	struct stat properties;
	
	if(stat(filename, &properties) == 0)
	{
		return (TRUE);
	}
	
	return (FALSE);
}

/* get_file_size()
 * 
 * Returns the size of a file.
 * */
unsigned int
get_file_size			(char * filename)
{
	struct stat properties;
	
	if (stat(filename, &properties) != 0)
	{
		log_message(WARNING, EMSG_STATFILE, NULL);
		return 0;
	}
	
	return (properties.st_size);
}

/* get_file_contents()
 * 
 * Read a file which size is below 2 Gb and return its contents as a
 * uint8_t array to avoid strange characters reading binary files. Value
 * returned should be freed after use.
 * */
uint8_t *
get_file_contents		(char * filename)
{
	int fd, size, ret;
	struct stat properties;
	uint8_t * contents;
	
	if ((fd = open(filename, O_RDONLY | O_NOATIME | O_NONBLOCK)) < 0)
	{
		/* Only the caller should know if this error is critical. Maybe
		 * failing one or two times is acceptable, as when only one file
		 * is really needed but many are readed.
		 * */
		log_message(WARNING, EMSG_OPENFILE, NULL);
		return (NULL);
	}
	
	if (stat(filename, &properties) != 0)
	{
		close(fd);
		log_message(WARNING, EMSG_STATFILE, NULL);
		return (NULL);
	}
	
	size = properties.st_size * sizeof(uint8_t);
	contents = malloc(size + 1);
	memset(contents, '\0', size + 1);

	/* Do not read the EOF character. */
	if ((ret = read(fd, contents, size)) < 0)
	{
		free(contents);
		close(fd);
		log_message(WARNING, EMSG_READFILE, NULL);
		return (NULL);
	}

	close(fd);
	return (contents);
}

/* check_supported_dir()
 * 
 * */
int
check_supported_dir			(char * path, char * id)
{
	char * full_path;
	struct stat properties;

	if ((asprintf(&full_path, "%s/%s", path, id) > 0) &&
		(stat(full_path, &properties) == 0))
	{
		
		free(full_path);
		return (S_ISDIR(properties.st_mode) && 
				(atoi(id) >= MINNUMPATH) && (atoi(id) <=  MAXNUMPATH));
	}

	if (full_path != NULL)
	{
		free(full_path);
	}
	return (0);
}

/* String functions.
 * 
 * Manipulate and compare strings.
 * */

/* get_between_delim()
 * 
 * Return the first 'str' substring between two delimiters.
 * The result should be freed after use.
 * */
char *
get_between_delim		(const char * str, char delim_head, char delim_tail)
{
	char * head, * tail, * res;
	int substr_len;
	
	res = NULL;
	
	if (((head = strchr(str, delim_head)) != NULL) &&
		(tail = strchr(head + 1, delim_tail)) != NULL)
	{
		substr_len = tail - head;
		res = malloc(substr_len * sizeof(char));
		snprintf(res, substr_len, "%s", head + 1);
	}
	
	return (res);
}


/* get_first_substr()
 * 
 * Return the 'str' substring before first 'delim'.
 * The result should be freed after use.
 * */
inline
char *
get_first_substr		(const char * str, char delim)
{
	char * ext, * res;
	int substr_len;
	
	res = NULL;
	
	if ((ext = strchr(str, delim)) != NULL)
	{
		substr_len = ext - str;
		res = malloc((substr_len + 1) * sizeof(char));
		snprintf(res, substr_len + 1, "%s", str);
	}
	
	return (res);
}

/* get_last_substr()
 * 
 * Return the substring after last 'delim'.
 * The result should be freed after use.
 * */
inline
char *
get_last_substr			(const char * str, char delim)
{
	char * res, * ext;
	
	res = NULL;

	if ((ext = strrchr(str, delim)) != NULL)
	{
		asprintf(&res, "%s", ext + 1);
	}
	
	return (res);
}

/* get_valid_substr_len()
 * 
 * Return the length of the first 'str' substring containing only
 * characters from 'valid_set', not counting the last '\0'.
 * This behaviour is pretty similar to strspn, but the latter seems broken
 * with some chars like '/'.
 * 
 * TODO: Optimize.
 * */
size_t
get_valid_substr_len		(const char * str, char * valid_set)
{
	int i, str_len, valid_set_len;
	size_t res;
	
	str_len = strlen(str);
	valid_set_len = strlen(valid_set);
	res = 0;
	
	for (i = 0; (i < str_len) && (res == i); i++)
	{
		if (strchr(valid_set, str[i]))
		{
			res++;
		}
	}

	return (res);
}

/* concat_and_free()
 * 
 * Concatenates two strings with memory reallocation and freeing.
 * Concatenates 'fst' and 'snd' strings, freeing them and returning
 * a newly allocated one.
 * */
char *
concat_and_free			(char * fst, char * snd)
{
	char * res;
	
	if (fst == NULL)
	{
		asprintf(&fst, "%s", "");
	}
	
	if (snd == NULL)
	{
		asprintf(&snd, "%s", "");
	}
	
	asprintf(&res, "%s%s", fst, snd);
	
	free(fst);
	free(snd);
	
	return (res);
}

/* find_and_replace()
 * 
 * Replaces the first occurrence of 'old' by 'new' in the string 'src',
 * and returns a newly allocated string that should be freed when it is
 * no longer needed. If the string 'old' is not found, it returns 'src'
 * unchanged.
 * IMPORTANT: 'src' cannot be a constant string or a statically allocated
 * one, as it is freed before the function ends.
 * */
void
find_and_replace		(char ** src, const char * old, const char * new)
{
	unsigned short src_len, old_len, head_len, tail_len;
	char * init;
	char * head, * tail;
	
	if ((*src) &&
		(init = strstr(*src, old)) != NULL)
	{
		src_len = strlen(*src) + 1;
		old_len = strlen(old);
	
		head_len = init - *src + 1;
		head = malloc(head_len * sizeof(char));
		snprintf(head, head_len, "%s", *src);

		tail_len = src_len - head_len - old_len + 1;
		tail = malloc(tail_len * sizeof(char));
		snprintf(tail, tail_len, "%s", init + old_len);

		free(*src);
		asprintf(src, "%s%s%s", head, new, tail);
		free(head);
		free(tail);
	}
}

/* find_vector_first()
 * 
 * Find the first occurrence of an element in a vector in a string,
 * and return the position of that element in the vector.
 * If the string does not exist, it returns -1.
 * */
inline
short
find_vector_first			(char * str, const char ** vector, size_t len)
{
	int i;
	
	/* NULL strings are errors. */
	if (str == NULL)
	{
		return (-1);
	}
	
	for (i = 0; i < len; i++)
	{
		/* Take care of empty strings first, then any other. */
		if ((str[0] == '\0') || (vector[i][0] == '\0'))
		{
			if ((str[0] == '\0') && (vector[i][0] == '\0'))
			{
				return (i);
			}
		}
		else
		{
			if (strncmp(str, vector[i], strlen(vector[i])) == 0)
			{
				return (i);
			}
		}
	}
	
	return (-1);
}

/* parse_key_value()
 * 
 * Parse key-value pairs in a string.
 * Returns an array with all the alphanumeric values, with maximun
 * length 'max_len', sorted according to the valid keys array. If a key has
 * not a value associated, a NULL value will be placed.
 * Currently, this function only support parsing with two delimiters
 * (usually '=' and '&' in URI and anything similar).
 * */
char **
parse_key_value		(char * str, const char ** keys, int len, char * delimiters, int max_len)
{
	int i, key_len, value_len, check_len;
	char * key, * valid_symbols;
	char ** res;
	const char * valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
	
	asprintf(&valid_symbols, "%s%s", valid_chars, delimiters);

	/* Allocate memory for the maximum size. */
	res = malloc(len * sizeof(char *));
	
	/* Assume the first delimiter found is the key-value delimiter, and
	 * the next one is the pair delimiter. For example, in this string:
	 *      data1=1&data2=2
	 * '=' is the key-value delimiter, and '&' is the pair delimiter.
	 * For many reasons, such as security issues or pragmatic programming,
	 * only the 2 first delimiters are used.
	 * */
	for (i = 0; i < len; i++)
	{
		res[i] = NULL;
		key = strstr(str, keys[i]);
		key_len = strlen(keys[i]);

		if ((key) &&
			(key[key_len] == delimiters[0]))
		{
			value_len = strchrnul(key, delimiters[1]) - (key + key_len);
			
			/* Truncate if the value length is higher than 'max_len'.*/
			if (value_len > max_len)
			{
				value_len = max_len;
			}
			
			res[i] = malloc(value_len * sizeof(char));
			snprintf(res[i], value_len, "%s", key + key_len + 1);
			
			/* Truncate if there is a non valid symbol. */
			if ((check_len = get_valid_substr_len(res[i], valid_symbols) + 1) < value_len)
			{
				free(res[i]);
				res[i] = malloc(check_len * sizeof(char));
				snprintf(res[i], check_len, "%s", key + key_len + 1);
			}
		}
	}
	
	free(valid_symbols);
	return (res);
}

/* wipe_special_chars()
 * 
 * Formats a string 'str' to print it on screen, truncating it if it is
 * necessary.
 * Result must be freed when it is no longer used.
 * */
char *
wipe_special_chars		(char * str, int len)
{
	int str_len, head_len, tail_len;
	char * head, * tail, * offset, * res, * special_str;
	char special_char;
	
	/* Chars to format. */
	const char * special_chars = "\b\t\n\v\f\r\e";

	/* Don't allow lengths over 'len'. */
	if ((len > 0) && (strlen(str) > len))
	{
		res = malloc((len + 5) * sizeof(char));
		snprintf(res, len, "%s", str);
		sprintf(res + len - 1, "[...]");
		str_len = strlen(res);
	}
	else
	{
		str_len = asprintf(&res, "%s", str);
	}

	while ((offset = strpbrk(res, special_chars)) != NULL)
	{
		special_char = offset[0];
		special_str = NULL;
		
		switch (special_char)
		{		
			case ('\b'):
				asprintf(&special_str, "\\b");
				break;
				
			case ('\t'):
				asprintf(&special_str, "\\t");
				break;
				
			case ('\n'):
				asprintf(&special_str, "\\n");
				break;
				
			case ('\v'):
				asprintf(&special_str, "\\v");
				break;
				
			case ('\f'):
				asprintf(&special_str, "\\f");
				break;
				
			case ('\r'):
				asprintf(&special_str, "\\r");
				break;
				
			case ('\e'):
				asprintf(&special_str, "\\e");
				break;
		}
		
		if (special_str != NULL)
		{
			head_len = offset - res + 1;
			head = malloc(head_len * sizeof(char));
			snprintf(head, head_len, "%s", res);
			
			tail_len = str_len - head_len + 1;
			tail = malloc(tail_len * sizeof(char));
			snprintf(tail, tail_len, "%s", res + head_len);
			
			free(res);
			str_len = asprintf(&res, "%s%s%s", head, special_str, tail);

			free(head);
			free(tail);
			free(special_str);
		}
	}
	
	return (res);
}

/* Search related functions.
 * 
 * This section has mostly helper functions for bsearch().
 * */
int
comp_int64_t			(const void * fst, const void * snd)
{
	return (*(int64_t *) fst - *(int64_t *) snd);
}

/* Atomic functions.
 * 
 * */

/* atomic_add_int()
 * 
 * Adds 1 to 'num' atomically.
 * */
inline
void
atomic_add_int		(int * num)
{
	__asm__ __volatile__ ("lock; addl $1,%0"
						: "=m" (*num)
						: "m" (*num));
}

/* atomic_sub_int()
 * 
 * Substracts 1 to 'num' atomically.
 * */
inline
void
atomic_sub_int		(int * num)
{
	__asm__ __volatile__ ("lock; subl $1,%0"
						: "=m" (*num)
						: "m" (*num));
}

/* atomic_set_int
 * 
 * */
inline
void
atomic_set_int		(int * num, int value)
{
	__asm__ __volatile__ ("lock; movl %1,%0"
						: "=m" (*num)
						: "r" (value));
}

/* System functions.
 * 
 * */

/* get_time()
 * 
 * Get current time in milliseconds from Epoch.
 * */
int64_t
get_time				()
{
	struct timespec cur_time;
	int64_t cur_epoch;
	
	if (clock_gettime(CLOCK_REALTIME, &cur_time) != 0)
	{
		return (0);
	}
	
	cur_epoch = (int64_t)cur_time.tv_sec * 1000000000 + (int64_t)cur_time.tv_nsec;
	return (cur_epoch);
}

/* get_server_ip()
 * 
 * Returned value must be freed after use.
 * */
const char *
get_server_name			()
{
	char * hostname;
	char * default_hostname;
	
	asprintf(&default_hostname, "localhost");
	
	if ((hostname = malloc(30 * sizeof(char))) == NULL)
	{
		return (default_hostname);
	}
	
	if (gethostname(hostname, 30) != 0)
	{
		return (default_hostname);
	}
	
	return (hostname);
}
