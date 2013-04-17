/* video_analysis.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <unistd.h>

#include "../server/common.h"
#include "file.h"
#include "video.h"

/* ********** Constant definitions ********** */

#define CHOPPER_VERSION		"1.1"
#define	FILENAME		"data.txt"
#define MAXNUMPATH		99999
#define MINNUMPATH		100

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
	   "\t-v, --version\t\t\t Print the application version\n"
	   "\t-p, --path\t\t\t Specify the path to analyse\n"
	   "\t-d, --dirs\t\t\t Watch for a specific set of directories in the path\n"
	);
}

/* check_supported_dir()
 * 
 * Check if a directory can be scanned. 
 * */
int
check_dir			(const struct dirent * file)
{
    return ((file->d_type == DT_DIR) && 
	    ((atoi(file->d_name) >= MINNUMPATH) && (atoi(file->d_name) <=  MAXNUMPATH)));
}

/* ********** Public functions ********** */


int
main			(int argc, char * argv[])
{
    int num_entries, i;
    char * path, * dirs, * dir, * filename;
    struct dirent ** entries;
	
    /* getopt_long() needed variables. */
    int next_opt;				/* Next option in getopt_long() */
    const char* short_opts = "hvp:d:";	/* Short options */
    const char* app_name = argv[0];		/* Name of the app */
	
    const struct option
    long_opts[] =
    {
	{ "help",    0,  NULL, 'h'},
	{ "version", 0,  NULL, 'v'},
	{ "path",    1,  NULL, 'p'},
	{ "dirs",    1,  NULL, 'd'},
	{ NULL,	     0,  NULL,  0}
    };
	
    path = NULL;
    dirs = NULL;
	
    /* Explore the options array */
    do
    {
	/* Call getopt_long. */
	next_opt = getopt_long (argc, argv, short_opts, long_opts, NULL);

	switch (next_opt)
	{
	    case 'h' :
		print_usage(app_name);
		exit(EXIT_SUCCESS);
				
	    case 'v' :
		printf("%s version: %s\n", app_name, CHOPPER_VERSION);
		exit(EXIT_SUCCESS);
				
	    case 'p' :
		asprintf(&path, "%s", optarg);
		break;
				
	    case 'd' :
		asprintf(&dirs, "%s", optarg);
		break;

	    case -1 : /* No more options. */
		break;

	    default:
		exit(EXIT_FAILURE);
	}
    }
    while (next_opt != -1);
	
    if (path == NULL)
    {
	fprintf(stderr, "You need to specify a path.\n");
	return (EXIT_FAILURE);
    }
	
    init_file(FILENAME);
	
    if (dirs == NULL)
    {
	/* Scan whole ads directory and allocate memory for all the entries found. */
	if ((num_entries = scandir(path, &entries, check_dir, alphasort)) < 1)
	{
	    fprintf(stderr, "Cannot scan directory: %s\n", path);
	    return (EXIT_FAILURE);
	}
		
	for (i = 0; i < num_entries; i++)
	{
	    asprintf(&filename, "%s/%s", path, entries[i]->d_name);
	    free(entries[i]);

	    if (!load_videos(filename))
	    {
		fprintf(stderr, "Scanning directory %s failed\n", filename);
	    }
			
	    free(filename);
	}
		
	/* Free memory. */
	free(entries);
    }
    else
    {
	dir = strtok(dirs, ",");
		
	/* Scan only a set of directories. */
	while (dir != NULL)
	{
	    asprintf(&filename, "%s/%s", path, dir);
			
	    if (!load_videos(filename))
	    {
		fprintf(stderr, "Scanning directory %s failed\n", filename);
	    }
			
	    dir = strtok(NULL, ",");
	    free(filename);
	}
		
    }
	
	
	
    exit_file();
	
    return (0);
	
}
