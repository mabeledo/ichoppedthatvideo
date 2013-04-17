/* Message module.
 * File: msg.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Define messages and codes.
 * Messages are constant char arrays, with a maximum size of 128 bytes.
 * Codes are integer values.
 * */

#ifndef MSG_H
#define MSG_H

#define MSG_MAXLENGTH		 128

/* ********** ichoppedthatvideo.c ********** */
#define EMSG_SULIMIT		"Failed setting the file size for core dumps"
#define ECOD_SULIMIT		-1

/* ********** common.c ********** */
#define EMSG_OPENFILE		"Cannot open the file"
#define ECOD_OPENFILE		-10
#define EMSG_STATFILE		"Cannot get stats from the file"
#define ECOD_STATFILE		-11
#define EMSG_READFILE		"Cannot read the file"
#define ECOD_READFILE		-12

/* ********** signal.c ********** */
#define EMSG_RULIMIT		"Cannot restore old ulimit"
#define ECOD_RULIMIT		-20
#define EMSG_SIGSEGV		"Received SIGSEGV signal"
#define ECOD_SIGSEGV		-21

#define IMSG_SIGTERM		"Received SIGTERM signal"
#define IMSG_SIGINT             "Received SIGINT signal"
#define IMSG_SIGDEFAULT         "Received a default signal"

/* ********** file.c ********** */
#define EMSG_ADPATH		"Error setting ads directory"
#define ECOD_ADPATH		-30
#define EMSG_ADIDUNK		"File ID not supported"
#define ECOD_ADIDUNK		-31
#define EMSG_FILEUNKNOWN	"File not supported"
#define ECOD_FILEUNKNOWN	-32
#define EMSG_BUILDPATH		"Error building file path"
#define ECOD_BUILDPATH		-33
#define EMSG_FILEPATH		"Error finding file"
#define ECOD_FILEPATH		-34

/* ********** request.c ********** */
#define EMSG_READSOCKET		"Error reading data from socket"
#define ECOD_READSOCKET		-40
#define EMSG_TYPEUNKNOWN	"Request type unknown"
#define ECOD_TYPEUNKNOWN	-41
#define EMSG_BADFORMREQ		"Bad formed request"
#define ECOD_BADFORMREQ		-42
#define EMSG_NOSOCKINFO		"Cannot get socket info"
#define ECOD_NOSOCKINFO		-43

#define IMSG_VALIDPARAM		"Valid parameters received"
#define IMSG_PARSING		"Parsing new request"

/* ********** reply.c ********** */
#define EMSG_COMPOSE		"Error composing reply"
#define ECOD_COMPOSE		-50
#define EMSG_COMPNOTFOUND	"Error composing a 'not found' reply"
#define ECOD_COMPNOTFOUND	-51

/* ********** stream.c ********** */
#define EMSG_FREEMEM		"Cannot determine free memory available"
#define ECOD_FREEMEM		-60
#define EMSG_VIDEOPATH		"Non valid path"
#define ECOD_VIDEOPATH		-61
#define EMSG_INVALSIGN		"Invalid signature"
#define ECOD_INVALSIGN		-62
#define EMSG_STREAMIDUNK	"Stream ID not supported"
#define ECOD_STREAMIDUNK	-63
#define EMSG_NOVIDEO		"No id or signature available"
#define ECOD_NOVIDEO		-64
#define EMSG_NOSTREAMAVAIL	"This video has no streams"
#define ECOD_NOSTREAMAVAIL	-65
#define EMSG_INVALVIDEO		"Invalid video selected"
#define ECOD_INVALVIDEO		-66
#define EMSG_INVALOFFSET	"Invalid iframe offset"
#define ECOD_INVALOFFSET	-67
#define EMSG_NOSTREAM		"Invalid stream"
#define ECOD_NOSTREAM		-68
#define EMSG_NODATAFILE		"This data file does not exist"
#define ECOD_NODATAFILE		-69

#define IMSG_INVALTIMEOUT       "Timeout too short, using default"
#define IMSG_NOVIDEO		"This path has no video files"
#define IMSG_VIDEOSTOP		"Video stopped before ending"
#define IMSG_CLIENTMSG		"Message received from client"
#define IMSG_SPENTTIME		"Time spent in last operation"
#define IMSG_BITRATELOW		"Bitrate lowered"
#define IMSG_BITRATEHIGH	"Bitrate raised"
#define IMSG_QUALITYSEL		"Quality selected by URL"
#define	IMSG_POSITIONSEL	"Position selected by URL"

/* ********** conn.c ********** */
#define EMSG_SOCKET		"Failed to create a socket"
#define ECOD_SOCKET		-70
#define EMSG_REUSEADDR		"Cannot set SO_REUSEADDR option"
#define ECOD_REUSEADDR		-71
#define EMSG_BIND		"Failed to bind the socket"
#define ECOD_BIND		-72
#define EMSG_LISTEN		"Failed listening on the socket"
#define ECOD_LISTEN		-73
#define EMSG_CREATELOCK		"Error creating lock"
#define ECOD_CREATELOCK		-74
#define EMSG_CHILDALLOC		"Error allocating memory for children"
#define ECOD_CHILDALLOC		-75
#define EMSG_CREATECHILD	"Error creating children"
#define ECOD_CREATECHILD	-76
#define EMSG_CHILDNOTFOUND	"Child not found"
#define ECOD_CHILDNOTFOUND	-77

#define IMSG_CONNINIT		"Now processing connections"
#define IMSG_THREADINIT		"New thread available"

/* ********** stat.c ********** */
#define EMSG_THREADSAFE		"MySQL libraries are not thread safe"
#define ECOD_THREADSAFE		-80
#define EMSG_DBHOST		"Host undefined"
#define ECOD_DBHOST		-81
#define EMSG_STATDBUSER		"User not defined for statistics database"
#define ECOD_STATDBUSER		-82
#define EMSG_STATDBPASS		"Password not defined for statistics database"
#define ECOD_STATDBPASS		-83
#define EMSG_DBSTATCONN		"Error connecting to statistics database"
#define ECOD_DBSTATCONN		-84
#define EMSG_GISTATINIT		"Error initializing GeoIP data"
#define ECOD_GISTATINIT		-85
#define EMSG_INSERT		"Error inserting data"
#define ECOD_INSERT		-86
#define EMSG_SELECT		"Error selecting data"
#define ECOD_SELECT		-87

#define IMSG_STATSENABLED	"Statistic module enabled"
#define IMSG_NEWREQSTAT		"New request registered"
#define IMSG_NEWSTRMSTAT	"New stream registered"
#define IMSG_STATCLOSED		"Statistic module closed"

/* ********** security.c ********** */
#define EMSG_BLACKLISTED	"Client already on blacklist"
#define ECOD_BLACKLISTED	-90
#define EMSG_ADDBLACKLIST	"Client added to blacklist"
#define ECOD_ADDBLACKLIST	-91

#define IMSG_SECENABLED		"Security module enabled"

/* ********** logging.c ********** */
#define EMSG_INVALIDLEVEL       "Invalid logging level"
#define ECOD_INVALIDLEVEL       -101
#define EMSG_INVALIDDEV         "Invalid logging device"
#define ECOD_INVALIDDEV         -102

#endif
