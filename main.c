/* $Id: main.c,v 1.13 2003/07/20 19:04:02 te Exp $ */

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
#include <string.h>

#if defined (_WINDOWS)
# include <windows.h>
# include <winsock.h>
int	ws_init(void);
void	ws_deinit(void);
#endif

#include <fh.h>
#include <url.h>
#include <http.h>
#include <getopt.h>

void		usage(char *);
extern int	resolv(const char *,struct in_addr *);
extern int	start_get(void *);

int debug;
int multi;	/* Multi threaded get */
int status_thr_disable;
int bench_time;
int conn_speed;
int thrs_num;
int ssl_auth_peer;
char *ssl_calist;
int ssl_method;

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct http_request *hr;
	struct proxy proxy;
	struct url_struct *proxyu;
	char *proxyp, *host;
	int ch;
	
	hr = newhttpreq();
	conn_speed = 1;
	thrs_num = -1;
	status_thr_disable = multi = debug = bench_time = 0;
	proxyp = NULL;
	proxyu = NULL;
	ssl_auth_peer = 0;
	ssl_calist = NULL;
	ssl_method = METHOD_SSL_V3;
	hr->hr_proxy = NULL;

	/* Parse command line option. */
	while ((ch = getopt(argc, argv, "p:dmsvbnt:M:AF:") ) != -1) {
		switch (ch) {
			case 'p':
				proxyp = optarg;
				break;
			case 'd':
				debug = 1;
				break;
			case 'm':
				multi = 1;
				break;
			case 's':
				status_thr_disable = 1;
				break;
			case 'v':
				printf("%s\n", VERSION);
				exit(0);
			case 'b':
				bench_time = 1;
				break;
			case 'n':
				conn_speed = 0;
				break;
			case 't':
				/*
				 * User settable number of threads.  But
				 * it's not recommended.
				 */
				thrs_num = atoi(optarg);
				break;
			case 'M':
				if (strcasecmp(optarg, "tlsv1") == 0)
					ssl_method = METHOD_TLS_V1;
				else if (strcasecmp(optarg, "sslv2") == 0)
					ssl_method = METHOD_SSL_V2;
				else if (strcasecmp(optarg, "sslv3") == 0)
					ssl_method = METHOD_SSL_V3;
				else {
					fprintf(stderr, "fh: Unknown SSL "
					    "method: %s.  Quitting\n", optarg);
					return (1);
				}
				break;
			case 'A':
				ssl_auth_peer = 1;
				break;
			case 'F':
				ssl_calist = optarg;
				break;
			case '?':
			default:
				usage("fh");
				exit(1);
		}
	}

	if (debug) {
#if !defined (DEBUG)
		fprintf(stderr,
		    "fh: Debug output requested; but program was\n"
		    "fh: not compiled with debug capabilities support.\n"
		    "fh: Please recompile with -DDEBUG flag.\n"
		    "fh: Debugging not enabled.\n");
		debug = 0;
#endif
	}

	(void)sock_startup(ssl_method, ssl_calist);

	if (thrs_num != -1) {
		if (multi == 0)
			fprintf(stderr, "fh: ignoring option `-t', not "
			    "multi-get\n");
		/* If the user supplied garbage let FH decide */
		if (thrs_num == 0 || thrs_num > MAX_GET_THREADS)
			thrs_num = -1;
	}

	if (argc <= 1)
		usage("fh");

	host = argv[argc-1];

	/*
	 * If there is no proxy server in command line look for
	 * `http_proxy' environment valiable
	 */
	if (proxyp == NULL) {
		proxyp = getenv("http_proxy");
	}

	if (proxyp != NULL) {
		if ((proxyu = parse_url(proxyp) ) == NULL) {
			fprintf(stderr, "main: cannot parse proxy URL %s",
			    proxyp);
			return (1);
		}
		proxy.p_host = proxyu->host;
		proxy.p_port = proxyu->port;
		proxy.p_authtype = P_AUTH_NONE;
		/* User name and password must be both set */
		if (proxyu->user != NULL && proxyu->passwd != NULL) {
			proxy.p_authtype = P_AUTH_BASIC;
			proxy.p_usr = proxyu->user;
			proxy.p_passwd = proxyu->passwd;
		}
		if (resolv(proxy.p_host, &proxy.p_addr) < 0) {
			fprintf(stderr, "main: resolv: %s: host unknown\n",
			    proxy.p_host);
			return (1);
		}
		hr->hr_proxy = &proxy;
#ifdef DEBUG
		if (debug)
			fprintf(stderr, "main: using HTTP proxy %s port %d\n",
			    proxy.p_host, proxy.p_port);
#endif
	}

#if defined (_WINDOWS)
	ws_init();
#endif

	hr->hr_url = parse_url(host);
	if (hr->hr_url == NULL) {
		fprintf(stderr, "main: cannot parse target URL %s",
		    host);
		return (1);
	}

	start_get(hr);

	/* Clean up */
	if (proxyu != NULL)
		ufree(proxyu);
	ufree(hr->hr_url);
	sock_exit();
#if defined (_WINDOWS)
	ws_deinit();
#endif
	return (0);
}

#if defined (_WINDOWS)
int
ws_init()
{
	unsigned short reqver;
	int err;
	WSADATA data;

	reqver = MAKEWORD(1, 1);

	err = WSAStartup(reqver, &data);
	if (err) {
		/*fprintf(stderr, "Failed to intialize winsock.dll\n");*/
		return (0);
	}
	return (1);
}

void
ws_deinit()
{

	WSACleanup();
	return;
}
#endif

void
usage(argv0)
	char *argv0;
{

	fprintf(stderr, 
	    "%s: usage: %s OPTIONS request_uri\n"
	    "  -p proxy      - specify proxy server\n"
	    "  -m            - start multi-get session\n"
	    "  -d            - enable debugging\n"
	    "  -s            - disable status thread (multi-get)\n"
	    "  -v            - print version number and exit\n"
	    "  -b            - enable benchmarcking\n"
	    "  -n            - do not show download speed\n"
	    "  -t num_thrs   - use <num_thrs> number of downloading\n"
	    "                  threads (multi-get)\n"
	    "  -M <sslv2|sslv3|tlsv1>   "
	    		    "- SSL method to use\n"
	    "  -A            - authenticate remote peer\n"
	    "  -F <file>     - path to file containing trusted CA's\n",
	    argv0, argv0);
	exit(0);
}
