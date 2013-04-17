/* Main module.
 * File: ichoppedthatvideo.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Main ichoppedthatvideo source file.
 * */

#define _GNU_SOURCE

#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "stat.h"
#include "security.h"
#include "signal.h"
#include "common.h"
#include "conn.h"
#include "stream.h"
#include "file.h"

#include "logging.h"

#define	SERVER_VERSION	      "1.0B1"
#define DEFAULT_CONFIG_DIR    "/etc/ichoppedthatvideo"
#define DEFAULT_PATH	      "/home/www/htdocs"

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
	   "\t-D, --daemonize\t\t\t Run the server on the background\n"
	   "\t-p, --path 'path'\t\t Files path. [Default: %s]\n"
	   "\t-a, --auth\t\t\t Require signed authentication while requesting video. [Default: Off]\n"
	   "\t-s, --stats\t\t\t Gather statistic data. [Default: Off]\n\n"
	   "System specific options\n"
	   "\t-P num, --port num\t\t Use the port 'port' to receive data [Default: %d]\n"
	   "\t-c num, --children num\t\t Create 'num' children processes [Default: %d]\n"
	   "\t-t num, --timeout num\t\t Set a timeout of 'num' seconds for each connection [Default: %d, mininum value: %d]\n"
	   "\t-C num, --closed-timeout num\t\t Set a timeout of 'num' seconds for closed connections [Default: off]\n\n"
	   "Debug specific options\n"
	   "\t-o 'output', --output 'output'\t Set the default log output: 'syslog', 'console' or 'both' [Default: %s]\n"
	   "\t-l num, --log-level num\t\t Define the minimum logging level, from more (1) to less (4) verbosity [Default: %d (Log only critical messages)]\n"
   	   "\t-d size, --dump-core size\t Set up the core dumping with a file size of 'size' [Default: dumping is off]\n\n"
	   "Security specific options\n"
	   "WARNING: Request dispatching performance will be possibly degradated.\n"
	   "\t-S, --security\t\t\t Activate security module\n"
	   "\t-R num, --request num\t\t Define maximum number of requests per second allowed [Default: %d]\n"
	   "\t-T num, --time num\t\t Define maximum time (in seconds) an IP can be blacklisted [Default: %d]\n"
	   "\t-B num, --blacklist num\t\t Set blacklist length to 'num' [Default: %d]\n",
	   DEFAULT_PATH, DEFAULT_PORT, DEFAULT_NUM_CHILDREN, DEFAULT_TIMEOUT, MIN_TIMEOUT, DEFAULT_OUTPUT, DEFAULT_LOG_LEVEL, 
	   DEFAULT_REQ_LIMIT, DEFAULT_TIME_LIMIT, DEFAULT_BLCK_LEN
	);
}

/* daemon_mode()
 * 
 * Details about this implementation are in Stevens/Rago 2005, pg. 426-428.
 * */
int
daemon_mode                     ()
{
    pid_t pid;
    struct sigaction sa;
    int i, fd_stdin, fd_stdout, fd_stderr;

    if ((pid = fork()) < 0)
    {
	return (EXIT_FAILURE);
    }
    else
    {
	if (pid != 0)
	{
	    /* Inside the parent. It must be closed. */
	    exit (EXIT_SUCCESS);
	}

	/* Inside the child. Become session leader. */
	if (setsid() < 0)
	{
	    return (EXIT_FAILURE);
	}
    }

    /* Ensure the daemon will not allocate a controlling TTY. */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, NULL) != 0)
    {

	return (EXIT_FAILURE);
    }

    if ((pid = fork()) < 0)
    {
	return (EXIT_FAILURE);
    }
    else
    {
	if (pid != 0)
	{
	    /* Inside the parent. */
	    exit (EXIT_SUCCESS);
	}
    }

    /* Attach stdin, stdout and stderr to /dev/null */
    for (i = 0; i < 3; i++)
    {
	close(i);
    }

    fd_stdin = open("/dev/null", O_RDWR);
    fd_stdout = dup(fd_stdin);
    fd_stderr = dup(fd_stdin);

    return (EXIT_SUCCESS);
}

/* dump_core()
 * 
 * Allows the application to dump cores up to size (int size).
 * */
void
dump_core			(int size)
{
    int proc_fd;
    struct rlimit core_limit;
    char * pattern = "%e@%h.core";

    core_limit.rlim_cur = (rlim_t)size;
    core_limit.rlim_max = (rlim_t)size;

    if ((setrlimit(RLIMIT_CORE, &core_limit) == 0) &&
	((proc_fd = open("/proc/sys/kernel/core_pattern", O_RDWR)) > 0))
    {
	write(proc_fd, pattern, sizeof(pattern));
	close(proc_fd);
    }
    else
    {
	log_message(WARNING, EMSG_SULIMIT, NULL);
    }
}

/* main()
 * 
 * */
int
main				(int argc, char* argv[])
{
    /* Inherited working directory. */
    char * old_wd;

    /* This will contain every function returned value. */
    int res;

    /* getopt_long() variables. */
    int next_opt;				                  /* Next option in getopt_long() */
    const char * short_opts = "hvDp:asP:c:t:C:o:l:d:SR:T:B:";     /* Short options */
    const char * app_name = argv[0];		                  /* Name of the app */
    int daemonize = 0;                                            /* Put the server on background. Default: Off. */
    char * path = DEFAULT_PATH;				          /* Path. Default: "/home/www/htdocs/" */
    int signed_auth = 0;                                          /* Need signed authentication. Default: Off. */
    int gather_stats = 0;			                  /* Gather statistic data. Default: Off. */
    int port = DEFAULT_PORT;				          /* Listening port. Default: 80 */
    int num_children = DEFAULT_NUM_CHILDREN;		          /* Number of child processes. Default: 192 */
    int timeout = DEFAULT_TIMEOUT;                                /* Data transfer timeout, in seconds. Default: 10 */
    int closed_timeout = DEFAULT_CLOSED_TO;                       /* Closed connection timeout, in seconds. Default: 0 (not active). */
    char * output = DEFAULT_OUTPUT;                               /* Logging output. Default: syslog. */
    int log_level = DEFAULT_LOG_LEVEL;			          /* Log level. Default: 4 (log only critical messages) */
    int core_size = 0;				                  /* Maximum file size on core dump, in bytes. */
    int security = 0;				                  /* Activate security module. Default: Off. */
    int req_limit = DEFAULT_REQ_LIMIT;			          /* Define maximum number of requests per second allowed. Default: DEFAULT_REQ_LIMIT. */
    int time_limit = DEFAULT_TIME_LIMIT;		          /* Define maximum time (in seconds) an IP can be blacklisted. Default: DEFAULT_TIME_LIMIT. */
    int blck_len = DEFAULT_BLCK_LEN;			          /* Define blacklist length. Default: DEFAULT_BLCK_LEN. */

    const struct option
    long_opts[] =
    {
	{ "help",      0,  NULL,   'h'},
	{ "version",   0,  NULL,   'v'},
	{ "daemonize", 0,  NULL,   'D'},
	{ "path",      1,  NULL,   'p'},
	{ "auth",      0,  NULL,   'a'},
	{ "stats",     0,  NULL,   's'},
	{ "port",      1,  NULL,   'P'},
	{ "children",  1,  NULL,   'c'},
	{ "timeout",   1,  NULL,   't'},
	{ "closed-timeout", 1, NULL, 'C'},
	{ "output",    1,  NULL,   'o'},
	{ "log-level", 1,  NULL,   'l'},
	{ "dump-core", 1,  NULL,   'd'},
	{ "security",  0,  NULL,   'S'},
	{ "requests",  1,  NULL,   'R'},
	{ "time",      1,  NULL,   'T'},
	{ "blacklist", 1,  NULL,   'B'},
	{ NULL,	       0,  NULL,    0}
    };
	

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
		printf("%s version: %s\n", app_name, SERVER_VERSION);
		exit(EXIT_SUCCESS);
	    
	    case 'D':
		daemonize = 1;
		break;
                        
	    case 'p' :
		asprintf(&path, "%s", optarg);
		break;

	    case 's' :
		gather_stats = 1;
		break;
		
	    case 'P' :
		port = atoi(optarg);
		break;

	    case 'a' :
		signed_auth = 1;
		break;
                    
	    case 'c' :
		num_children = atoi(optarg);
		break;

	    case 't':
		timeout = atoi(optarg);
		break;

	    case 'C':
		closed_timeout = atoi(optarg);
		break;

	    case 'o':
		asprintf(&output, "%s", optarg);
		break;                    

	    case 'l' :
		log_level = atoi(optarg);
		break;

	    case 'd' :
		core_size = atoi(optarg);
		break;
		    
	    case 'S' :
		security = 1;
		break;
                    
	    case 'R' :
		req_limit = atoi(optarg);
		break;
                    
	    case 'T' :
		time_limit = atoi(optarg);
		break;
		    
	    case 'B' :
		blck_len = atoi(optarg);
		break;
		    
	    case -1 : /* No more options. */
		break;
		    
	    default:
		printf("Invalid option.\n\n");
		print_usage(app_name);
		exit(EXIT_FAILURE);
	}
    }
    while (next_opt != -1);
    
    /* Here should be a permission check, as only admin users can run ichoppedthatvideo. */

    /* Change the working directory. */
    if ((old_wd = get_current_dir_name()) != NULL)
    {
	if (chdir(DEFAULT_CONFIG_DIR) != 0)
	{
	    return (EXIT_FAILURE);
	}
    }
    else
    {
	return (EXIT_FAILURE);
    }

    /* Initialize the server logger before any subsystem. */
    if ((res = init_log(log_level, output)) != EXIT_SUCCESS)
    {
	return (res);
    }

    if (strcmp(output, DEFAULT_OUTPUT) != 0)
    {
	free(output);
    }
    
    /* Daemonize. */
    if (daemonize)
    {
	printf("Daemonizing process.\nMessages won't be displayed on terminal after this point.\n");
	
	if ((res = daemon_mode()) != EXIT_SUCCESS)
	{
	    return (res);
	}
    }
    else
    {
	printf("Server configuration summary:\n");
    }

    /* Manage signals. */
    signal(SIGSEGV, default_handler);		/* SEGV, like a segmentation fault. */
    signal(SIGINT, default_handler);		/* INT, like Ctrl + C .*/
    signal(SIGTERM, default_handler);
	
    /* signal(SIGCHLD, sigchild_handler);	 CHLD, returned by a child process. */
    /* signal(SIGKILL, default_handler);	 KILL, like kill -9. */

    /* Configure core dumping. */
    if (core_size)
    {
	dump_core(core_size);
	printf("\t-> Core dumping enabled, core size: %d bytes.\n", core_size);
    }

    /* Initialize modules.
     * */

    /* Initialize file module. */
    if ((res = init_file(path)) != EXIT_SUCCESS)
    {
	return (res);
    }

    /* Initialize video management. */
    if ((res = init_videos(path, signed_auth, timeout)) != EXIT_SUCCESS)
    {
	return (res);
    }

    printf("\t-> Checked file path '%s'.\n", path);
    
    if (strcmp(path, DEFAULT_PATH) != 0)
    {
	free(path);
    }

    /* Initialize db stats. */
    if (gather_stats)
    {
	if ((res = init_stat("localhost", NULL)) != EXIT_SUCCESS)
	{
	    return (res);
	}
	printf("\t-> Statistics module enabled.\n");
    }
	
    /* Initialize security module. */
	
    if (security)
    {
	if ((res = init_security(req_limit, time_limit, blck_len)) != EXIT_SUCCESS)
	{
	    return (res);
	}
	printf("\t-> Security module enabled."
               "\n\t\t-> Request limit: %d req/sec."
               "\n\t\t-> Ban time limit: %d sec." 
               "\n\t\t-> Backlist length: %d\n", req_limit, time_limit, blck_len);
    }
	
    /* Initialize children. */
    if ((res = init_conn(port, num_children, closed_timeout)) != EXIT_SUCCESS)
    {
	return(res);
    }

    printf("\t-> Server up & running in port %d with %d threads\n", port, num_children);

    while (TRUE)
    {
	pause();
    }

    chdir(old_wd);
    free(old_wd);
    return (EXIT_SUCCESS);
}
