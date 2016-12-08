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

/* debug printer for the parser */
static void nputs(char *str, int n)
{
	for (int i = 0; i < n; ++i) {
		printf("%c", str[i]);
	}
	printf("\n");
}

/* count how many 'chr' characters are in a string */
inline int count_chr(char *str, int str_len, char chr)
{
	int count = 0;
	for (int i = 0; i < str_len; ++i) {
		if (str[i] == chr)
			++count;
	}
	return count;
}

static int parse_address(char *str, int str_len, 
	AVal *host, unsigned int *port)
{
	int port_num;
	char port_str[6];
	int colon_count = count_chr(str, str_len, ':');
	if (colon_count == 1) { // port included
		// address
		host->av_val = str;
		host->av_len = strchr(str, ':') - str;
		// port
		if (str_len - host->av_len - 1 > 5) // invalid port
			return FALSE;
		strncpy(port_str, str + host->av_len + 1, 
			str_len - host->av_len - 1);
		port_num = atoi(port_str);
		if (port_num < 1 || 65535 < port_num) // invalid port
			return FALSE;
		*port = port_num;
	} else if (colon_count == 0) { // no port included
		host->av_val = str;
		host->av_len = str_len;
	} else { // invalid address
		return FALSE;
	}
	return TRUE;
}

/* new standard confornment parser */
int RTMP_ParseURL(const char *url, int *protocol, AVal *host,
 unsigned int *port, AVal *playpath, AVal *app)
{
	char *pointer = strstr(url, "://");
	int protocol_len = pointer - url;
	if(protocol_len == 4 && 0 ==
	 strncasecmp(url, "rtmp", 4)) {
		*protocol = RTMP_PROTOCOL_RTMP;
	} else if(protocol_len == 5 && 0 ==
	 strncasecmp(url, "rtmpt", 5)) {
		*protocol = RTMP_PROTOCOL_RTMPT;
	} else if(protocol_len == 5 && 0 ==
	 strncasecmp(url, "rtmps", 5)) {
	    *protocol = RTMP_PROTOCOL_RTMPS;
	} else if(protocol_len == 5 && 0 ==
	 strncasecmp(url, "rtmpe", 5)) {
	    *protocol = RTMP_PROTOCOL_RTMPE;
	} else if(protocol_len == 5 && 0 ==
	 strncasecmp(url, "rtmfp", 5)) {
	    *protocol = RTMP_PROTOCOL_RTMFP;
	} else if(protocol_len == 6 && 0 ==
	 strncasecmp(url, "rtmpte", 6)) {
	    *protocol = RTMP_PROTOCOL_RTMPTE;
	} else if(protocol_len == 6 && 0 ==
	 strncasecmp(url, "rtmpts", 6)) {
	    *protocol = RTMP_PROTOCOL_RTMPTS;
	} else {
		RTMP_Log(RTMP_LOGWARNING, "Unknown protocol!\n");
		return FALSE;
	}
	pointer += 3; // skip ://

	int slash_count = count_chr(pointer, strlen(pointer), '/');
	if (slash_count > 2) { // long url
		for (int i = 0;;++i) {
			char *next = strchr(pointer, '/');
			int len = next - pointer;
			if (!next)
				len = strlen(pointer);
			switch (i) {
			case 0: // host
				if (!parse_address(pointer,
				 len, host, port)) { // invalid address
					return FALSE;
				}
				break;
			case 1: // app
				app->av_val = pointer;
				break;
			case 2: // app instance
				app->av_len = next - app->av_val;
				break;
			case 3: // playpath from now onwards
				playpath->av_val = pointer;
				playpath->av_len = strlen(pointer);
				goto done;
			}
			if (!next) // invalid url
				return FALSE;
			pointer = next + 1;
		}
	} else if (slash_count == 2 ) { // short url
		for (int i = 0;;++i) {
			char *next = strchr(pointer, '/');
			int len = next - pointer;
			if (!next)
				len = strlen(pointer);
			switch (i) {
			case 0: // host
				if (!parse_address(pointer,
				 len, host, port)) { // invalid address
					return FALSE;
				}
				break;
			case 1: // app
				app->av_val = pointer;
				app->av_len = len;
				break;
			case 2: // playpath
				playpath->av_val = pointer;
				playpath->av_len = len;
				break;
			default: // invalid url
				return FALSE;
			}
			if (!next)
				break;
			pointer = next + 1;
		}
	} else { // invalid url
		return FALSE;
	}
done:

	return TRUE;
}

int RTMP_ParseURL_old(const char *url, int *protocol,
 AVal *host, unsigned int *port, AVal *playpath, AVal *app)
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
