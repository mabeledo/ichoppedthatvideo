/* get_sign.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * */

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>	

#include "../server/common.h"

/* ********** Constant definitions ********** */

#define	FILENAME			"data.txt"

/* ********** Private functions ********** */

/* print_usage()
 * 
 * Prints the available options on the command line.
 * */
void
print_usage			(const char * app_name)
{
	printf("Usage: %s [OPTIONS]\n", app_name);
	printf("Available options:\n"
		   "\t-h, --help\t\t\t Display this usage information\n"
		   "\t-p, --path\t\t\t Specify the sign path\n"
		   );
}

/* ********** Public functions ********** */

int
main			(int argc, char * argv[])
{
	char * path, * filename, * offset, * sign, * file_data;
	
	/* getopt_long() needed variables. */
	int next_opt;								/* Next option in getopt_long() */
	const char* short_opts = "hp:";				/* Short options */
	const char* app_name = argv[0];				/* Name of the app */
	
	const struct option
	long_opts[] =
	{
		{ "help",		0,  NULL,   'h'},
		{ "path",		1,  NULL,	'p'},
		{ NULL,			0,  NULL,   0  }
	};
	
	path = NULL;
	
	/* Explore the options array */
	do
	{
		/* Call getopt_long. */
		next_opt = getopt_long (argc, argv, short_opts, long_opts, NULL);

		switch (next_opt)
		{
			case 'h' :
				print_usage(app_name);
				return(EXIT_SUCCESS);
				
			case 'p' :
				asprintf(&path, "%s", optarg);
				break;

			case -1 : /* No more options. */
				break;

			default:
				return(EXIT_FAILURE);
		}
	}
	while (next_opt != -1);
	
	if (path == NULL)
	{
		fprintf(stderr, "You need to specify a path.\n");
		return (EXIT_FAILURE);
	}

	asprintf(&filename, "%s/%s", path, FILENAME);
	file_data = (char *)get_file_contents(filename);
	free(filename);
	
	offset = strchr(file_data, '\n') + 1;
	sign = get_first_substr(offset, '\n');
	printf("SHA-1 sign: %s\n", sign);
	
	free(file_data);
	free(sign);
	return (EXIT_SUCCESS);
}
