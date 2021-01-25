/* $Id: schemes.c,v 1.2 2003/07/20 19:04:02 te Exp $ */

/*
 * Copyright (c) 2002 Tamer Embaby <tsemba@menanet.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include <url.h>

/*
 * Retrun codes 1 OK, 0 error in data, -1 scheme doesn't support
 * this component.
 */

/*
 * Com: URL component decriptor.
 * data: URL component.
 */
int
sch_http(data, com)
	void *data;
	int com;
{
	char *p = (char *)data;

	if (!p || strlen(p) == 0)
		return 0;

	switch (com) {
		case US_SCHEME:
			if (strcmp(p, "http") )
				return 0;
			break;
		case US_USER:
		case US_PASSWD:
			fprintf(stderr, "sch_ftp: (%s) unsupported in HTTP"
			    " scheme\n", p);
			return -1;
		case US_PORT:
		{
			u_short port;

			port = (u_short)atoi(p);
			if (port <= 0 || port > 65536)
				return 0;
			break;
		}
		case US_HOST:
		case US_PATH:
		case US_PARAMS:
		case US_QUERY:
		case US_FRAG:
			break;
		default:
			return 0;
	}
	return 1;
}

int
sch_file(data, com)
	void *data;
	int com;
{
	char *p = (char *)data;

	if (!p || strlen(p) == 0)
		return 0;

	switch (com) {
		case US_SCHEME:
			if (strcmp(p, "file") )
				return 0;
			break;
		case US_USER:
		case US_PASSWD:
			return -1;
		case US_PORT:
		{
			int port;

			port = atoi(p);
			if (port <= 0 || port > 65536)
				return 0;
			break;
		}
		case US_HOST:
		case US_PATH:
		case US_PARAMS:
		case US_QUERY:
		case US_FRAG:
			break;
		default:
			return 0;
	}
	return 1;
}

int
sch_ftp(data, com)
	void *data;
	int com;
{
	char *p = (char *)data;

	if (!p || strlen(p) == 0)
		return 0;

	switch (com) {
		case US_SCHEME:
			if (strcmp(p, "ftp") )
				return 0;
			break;
		case US_PORT:
		{
			int port;

			port = atoi(p);
			if (port <= 0 || port > 65536)
				return 0;
			break;
		}
		case US_USER:
		case US_PASSWD:
		case US_HOST:
		case US_PATH:
			break;
		case US_PARAMS:
		case US_QUERY:
		case US_FRAG:
			fprintf(stderr, "sch_ftp: (%s) unsupported in FTP"
			    " scheme\n", p);
			return -1;
		default:
			return 0;
	}

	return 1;
}

int
sch_telnet(data, com)
	void *data;
	int com;
{
	char *p = (char *)data;

	if (!p || strlen(p) == 0)
		return 0;

	switch (com) {
		case US_SCHEME:
			if (strcmp(p, "telnet") )
				return 0;
			break;
		case US_PORT:
		{
			int port;

			port = atoi(p);
			if (port <= 0 || port > 65536)
				return 0;
			break;
		}
		case US_HOST:
			break;
		case US_PATH:
		case US_USER:
		case US_PASSWD:
		case US_PARAMS:
		case US_QUERY:
		case US_FRAG:
			return -1;
		default:
			return 0;
	}

	return 1;
}

/*
 * HTTPS URL verfier.
 */
int
sch_https(data, com)
	void	*data;
	int	com;
{
	char *p;
       
	p = data;
	if (p == NULL || strlen(p) == 0)
		return (0);

	switch (com) {
		case US_SCHEME:
			if (strcmp(p, "https") != 0)
				return (0);
			break;
		case US_USER:
		case US_PASSWD:
			fprintf(stderr,
			    "sch_https: username/passwd (%s) is not valid in "
			    "HTTPS URL\n", p);
			return (-1);
		case US_PORT:
		{
			u_short port;

			port = (u_short)atoi(p);
			if (port <= 0 || port > 65536)
				return (0);
			break;
		}
		case US_HOST:
		case US_PATH:
		case US_PARAMS:
		case US_QUERY:
		case US_FRAG:
			break;
		default:
			return (0);
	}
	return (1);
}
