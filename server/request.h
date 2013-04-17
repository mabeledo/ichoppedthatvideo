/* Request module.
 * File: request.h
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 * 
 * Manage requests from clients.
 * */

#ifndef REQUEST_H
#define REQUEST_H

/* ********** Constant definitions. ********** */

/* Define request parameters.
 * Each request has a type (ad, video, etc) and a parameter set
 * (id, size, etc). Here, each parameter is defined by a name and a
 * code.
 * */
#define VIDEOID_PARAM_CODE			0
#define SETID_PARAM_CODE		        1
#define CLIENTID_PARAM_CODE			2
#define DATA_PARAM_CODE				3
#define INTRO_PARAM_CODE			4
#define SIGN_PARAM_CODE				5
#define QUALITY_PARAM_CODE			6
#define POS_PARAM_CODE				7
#define CACHE_PARAM_CODE			8

#define VIDEOID_PARAM_NAME			"video_id"
#define SETID_PARAM_NAME		        "setid"
#define CLIENTID_PARAM_NAME			"clientid"
#define DATA_PARAM_NAME				"data"
#define INTRO_PARAM_NAME			"intro"
#define SIGN_PARAM_NAME				"sign"
#define QUALITY_PARAM_NAME			"quality"
#define POS_PARAM_NAME				"pos"
#define CACHE_PARAM_NAME			"cache"

/* ********** Public functions ********** */
int
manage_request			(int client_fd);

#endif
