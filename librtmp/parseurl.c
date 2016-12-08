/*
 *  Copyright (C) 2009 Andrej Stepanchuk
 *  Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <ctype.h>

#include "rtmp_sys.h"
#include "log.h"

int RTMP_ParseURL(const char *url, int *protocol, AVal *host, unsigned int *port,
	AVal *playpath, AVal *app)
{
	char *p, *end, *col, *ques, *slash;

	RTMP_Log(RTMP_LOGDEBUG, "Parsing...");

	*protocol = RTMP_PROTOCOL_RTMP;
	*port = 0;
	playpath->av_len = 0;
	playpath->av_val = NULL;
	app->av_len = 0;
	app->av_val = NULL;

	/* Old School Parsing */

	/* look for usual :// pattern */
	p = strstr(url, "://");
	if(!p) {
		RTMP_Log(RTMP_LOGERROR, "RTMP URL: No :// in url!");
		return FALSE;
	}
	{
	int len = (int)(p-url);

	if(len == 4 && strncasecmp(url, "rtmp", 4)==0)
		*protocol = RTMP_PROTOCOL_RTMP;
	else if(len == 5 && strncasecmp(url, "rtmpt", 5)==0)
		*protocol = RTMP_PROTOCOL_RTMPT;
	else if(len == 5 && strncasecmp(url, "rtmps", 5)==0)
	        *protocol = RTMP_PROTOCOL_RTMPS;
	else if(len == 5 && strncasecmp(url, "rtmpe", 5)==0)
	        *protocol = RTMP_PROTOCOL_RTMPE;
	else if(len == 5 && strncasecmp(url, "rtmfp", 5)==0)
	        *protocol = RTMP_PROTOCOL_RTMFP;
	else if(len == 6 && strncasecmp(url, "rtmpte", 6)==0)
	        *protocol = RTMP_PROTOCOL_RTMPTE;
	else if(len == 6 && strncasecmp(url, "rtmpts", 6)==0)
	        *protocol = RTMP_PROTOCOL_RTMPTS;
	else {
		RTMP_Log(RTMP_LOGWARNING, "Unknown protocol!\n");
		goto parsehost;
	}
	}

	RTMP_Log(RTMP_LOGDEBUG, "Parsed protocol: %d", *protocol);

parsehost:
	/* let's get the hostname */
	p+=3;

	/* check for sudden death */
	if(*p==0) {
		RTMP_Log(RTMP_LOGWARNING, "No hostname in URL!");
		return FALSE;
	}

	end   = p + strlen(p);
	col   = strchr(p, ':');
	ques  = strchr(p, '?');
	slash = strchr(p, '/');

	{
	int hostlen;
	if(slash)
		hostlen = slash - p;
	else
		hostlen = end - p;
	if(col && col -p < hostlen)
		hostlen = col - p;

	if(hostlen < 256) {
		host->av_val = p;
		host->av_len = hostlen;
		RTMP_Log(RTMP_LOGDEBUG, "Parsed host    : %.*s", hostlen, host->av_val);
	} else {
		RTMP_Log(RTMP_LOGWARNING, "Hostname exceeds 255 characters!");
	}

	p+=hostlen;
	}

	/* get the port number if available */
	if(*p == ':') {
		unsigned int p2;
		p++;
		p2 = atoi(p);
		if(p2 > 65535) {
			RTMP_Log(RTMP_LOGWARNING, "Invalid port number!");
		} else {
			*port = p2;
		}
	}

	if(!slash) {
		RTMP_Log(RTMP_LOGWARNING, "No application or playpath in URL!");
		return TRUE;
	}
	p = slash+1;

	{
	/* parse application
	 *
	 * rtmp://host[:port]/app[/appinstance][/...]
	 * application = app[/appinstance]
	 */

	char *slash2, *slash3 = NULL, *slash4 = NULL;
	int applen, appnamelen;

	slash2 = strchr(p, '/');
	if(slash2)
		slash3 = strchr(slash2+1, '/');
	if(slash3)
		slash4 = strchr(slash3+1, '/');

	applen = end-p; /* ondemand, pass all parameters as app */
	appnamelen = applen; /* ondemand length */

	if(ques && strstr(p, "slist=")) { /* whatever it is, the '?' and slist= means we need to use everything as app and parse plapath from slist= */
		appnamelen = ques-p;
	}
	else if(strncmp(p, "ondemand/", 9)==0) {
                /* app = ondemand/foobar, only pass app=ondemand */
                applen = 8;
                appnamelen = 8;
        }
	else { /* app!=ondemand, so app is app[/appinstance] */
		if(slash4)
			appnamelen = slash4-p;
		else if(slash3)
			appnamelen = slash3-p;
		else if(slash2)
			appnamelen = slash2-p;

		applen = appnamelen;
	}

	app->av_val = p;
	app->av_len = applen;
	RTMP_Log(RTMP_LOGDEBUG, "Parsed app     : %.*s", applen, p);

	p += appnamelen;
	}

	if (*p == '/')
		p++;

	playpath->av_val = p;
	playpath->av_len = end - p;

	return TRUE;
}
