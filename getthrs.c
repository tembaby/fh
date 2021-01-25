/* $Id: getthrs.c,v 1.11 2003/07/20 19:04:01 te Exp $ */

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
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined (_WINDOWS)
# include <time.h>
# include <direct.h>
#else
# include <sys/time.h>
#endif

#if defined (UNIX)
# include <unistd.h>
#endif

#include <url.h>
#include <http.h>

extern int status_thr_disable;

struct thr_context mghttpsw[MAX_GET_THREADS];
struct timeval zero;

int	prepare_chunks_dir(char *);
int	prepare_chunk_output(struct thr_context *,char *,int);
int	getnthrds(int);
int	getchunksiz(int,int);
int	inittime(struct timeval *);
long	usecs();
int	copy_all(struct thr_context *,int,char *);
int	getoutputdir(char*,int,char *, char *);

extern	int getoutputfile(char *,int,char *,char *);

/*
 * pthread_create:	(hThread) CreateThread(NULL, 0,
 *				(LPTHREAD_START_ROUTINE)client_thr,
 *				data, 0, &thread_id) )
 * pthread_join:	WaitFor[Single|Multiple]Object()
 * pthread_exit:	ExitThread(dwResult)
 * pthread_kill?
 * pthread_self:	HANDLE GetCurrentThread(VOID);
 * pthread_cancel:	BOOL TerminateThread(hThread, dwExitCode)
 * pthread_sigmask? -> sigprocmask
 */

#if defined (_WINDOWS)
typedef u_int	__thread_return;
# define THREAD_NULL	0
#else	/* !_WINDOWS */
typedef void	*__thread_return;
# define THREAD_NULL	NULL
#endif

__thread_return	httpfetch_thr(void *);
__thread_return	status_thr(void  *);

int
start_get(http)
	struct http_request *http;
{
	int i, err, nr_thrds, totsiz;
	int chunksiz, remain;
	long usec;
	pthread_t watchcat;
	struct thr_context *hctx;
	struct http_request *httpr;
	struct thr_context thr_dummy;
	struct resume_pair rp_resume;
	char ofile[PATH_MAX];
	char odir[PATH_MAX];
#if defined (_WINDOWS)
	u_int thrdummyid;
	HANDLE hthrs[MAX_GET_THREADS];

	err = 0;
#endif

	/* Single mode requested. */
	if (multi == 0)
		return (http_single(http) );

	inittime(&zero);
	memset(&rp_resume, 0, sizeof(struct resume_pair));

#ifdef DEBUG
	if (debug)
		fprintf(stderr, "start_get: Fetching %s from %s\n",
		    http->hr_url->path ? http->hr_url->path : "ROOT",
		    http->hr_url->host);
#endif

	nr_thrds = thrs_num;

	http->hr_meth = HTTP_HEAD;
	http->hr_htver = MK_VER(1, 1);

	/* Now try to get the size of the HTTP entity (Content-Length). */
	thr_dummy.tc_httpr = http;
	thr_dummy.tc_seq = 0;
	if (http_get_info(&thr_dummy) < 0) {
		fprintf(stderr, "start_get: Resume not supported, possibly pre-"
		    "HTTP/1.1 server.  Use single mode\n");
#if 0
		return (http_single(http) );
#endif
		return (-1);
	}

	totsiz = http->hdrs.content_length;
	if (totsiz < 0) {
		fprintf(stderr, "start_get: HTTP/1.1 server never give us "
		    "Content-Length.  Object of unknown size!\n");
		return (http_single(http) );
	}

	/*
	 * Get approperiate threads count, and app. chunk size for
	 * each one of them
	 */
	if (thrs_num == -1)
		nr_thrds = getnthrds(totsiz);
	if (nr_thrds == 1) {
#ifdef DEBUG
		if (debug > 0)
			fprintf(stderr, "start_get: nr_threads=1, switching "
			    "to single_get\n");
#endif
		return (http_single(http) );
	}

	getoutputdir(odir, sizeof(odir),
	    http->hr_url->path, http->hr_url->host);

	/* Prepare the resuming information */
	RS_SAVE(&rp_resume)->rs_thrcnt = nr_thrds;
	RS_SAVE(&rp_resume)->rs_size = totsiz;
	RS_SAVE(&rp_resume)->rs_url = http->hr_url->url;
	RS_SAVE(&rp_resume)->rs_dir = odir;

	chunksiz = getchunksiz(nr_thrds, totsiz);

#ifdef DEBUG
	if (debug > 0)
		fprintf(stderr, "start_get: total %d, "
		    "chunk %d, %d thread%s.\n", totsiz, chunksiz,
		    nr_thrds, nr_thrds > 1 ? "s" : "");
#endif

	remain = totsiz;

	/* Calculate approp. chunk (portion) size for each thread. */
#define CHUNK_START(h)	(h->tc_seq * h->tc_totsiz)
#define CHUNK_END(h)	((h->tc_offset.start + h->tc_totsiz) - 1)
	for (i = 0; i < nr_thrds; i++) {
		hctx = &mghttpsw[i];
		hctx->tc_seq = i;
		hctx->tc_fmeth = FETCH_MULTIPLE;

		if (chunksiz > remain)
			chunksiz = remain;
		hctx->tc_totsiz = chunksiz;
		remain -= chunksiz;
		hctx->tc_offset.start = CHUNK_START(hctx);

		if ((i + 1) == nr_thrds) {
			/* Last thread */
			if (remain != 0)
				hctx->tc_totsiz += remain;
		}
		hctx->tc_offset.end = CHUNK_END(hctx);

		httpr = newhttpreq();

		/* Version 1.1 and method GET */
		httpr->hr_meth = HTTP_GET;
		httpr->hr_htver = MK_VER(1, 1);

		/* Common fields in HTTP request structure */
		httpr->hr_url = http->hr_url;
		httpr->hr_proxy = http->hr_proxy;

		hctx->tc_httpr = httpr;

		prepare_chunk_output(hctx, odir, i);
#ifdef DEBUG
		if (debug > 0)
			fprintf(stderr, "start_get: prepare thread %d -> %s "
			    "range: %d-%d\n", hctx->tc_seq, hctx->tc_tfil,
			    hctx->tc_offset.start, hctx->tc_offset.end);
#endif
		/* Set the ranges for every thread in resuming info */
		RS_SAVE(&rp_resume)->rs_ranges[i].start = hctx->tc_offset.start;
		RS_SAVE(&rp_resume)->rs_ranges[i].end = hctx->tc_offset.end;
	}

	/* Create a temporary chunks directory if it's not there */
	if (prepare_chunks_dir(odir) < 0)
		return (-1);

	RS_READ(&rp_resume)->rs_dir = odir;
	RS_READ(&rp_resume)->rs_thrcnt = 0;
	if (/* continue flag? || */ resume_read(RS_READ(&rp_resume)) == 0) {
		if (resume_consistent(&rp_resume) < 0) {
			fprintf(stderr, "start_get: resume info not in sync\n");
			resume_dprint(&rp_resume);
			return (-1);
		}
		/* Copy back the correct starting offset */
		for (i = 0; i < RS_READ(&rp_resume)->rs_thrcnt; i++) {
			hctx = &mghttpsw[i];
			/* Offset *within* the chunk */
			hctx->tc_offset.start +=
			    RS_READ(&rp_resume)->rs_ranges[i].cstart;
			/*
			 * Subtract the previously downlowded chunk size
			 * from total size so it get displayed from status
			 * thread `watchcat' correctly (tc_totsiz).  It's
			 * done this way instead of incrementing `tc_cursiz'
			 * because tc_cursiz get mangled in http_contents().
			 */
			hctx->tc_totsiz -=
			    RS_READ(&rp_resume)->rs_ranges[i].cstart;
#ifdef DEBUG
			if (debug > 0)
				printf("debug: thread %d restart off %d\n", i,
				    hctx->tc_offset.start);
#endif
		}
	} else {
		(void) resume_save(RS_SAVE(&rp_resume));
	}

	usec = usecs();
	if (status_thr_disable == 0) {
#if defined (_WINDOWS)
		if ((watchcat = CreateThread(NULL, 0,
		    (LPTHREAD_START_ROUTINE)status_thr, NULL, 0,
		    &thrdummyid) ) == NULL) {
			fprintf(stderr, "thr_create: watch_cat: (%d)",
			    GetLastError() );
			exit(1);
		}
#else	/* !_WINDOWS */
		err = pthread_create(&watchcat, NULL, status_thr, NULL);
		if (err != 0) {
			perror("thr_create: watch_cat");
			exit(1);
		}
#endif
	}

	/* Spawn the threads */
	for (i = 0; i < nr_thrds; i++) {
		hctx = &mghttpsw[i];
		hctx->tc_flags = TC_HTTP_PARTIAL;
		/*
		 * If we only have one thread we avoid using
		 * partial gets.
		 */
		if (nr_thrds == 1) {
			hctx->tc_flags = TC_HTTP_COMPLETE;
		}

#if defined (_WINDOWS)
		if ((hctx->tc_id = CreateThread(NULL, 0,
		    (LPTHREAD_START_ROUTINE)httpfetch_thr, hctx, 0,
		    &thrdummyid) ) == NULL) {
			fprintf(stderr, "thr_create: thread%d: (%d)\n",
			    i, GetLastError() );
			exit(1);
		}
		hthrs[i] = hctx->tc_id;
#else	/* !_WINDOWS */
		err = pthread_create(&hctx->tc_id, NULL,
		    httpfetch_thr, hctx);
		if (err != 0) {
			perror("thr_create");
			exit(1);
		}
#endif
	}

#if defined (_WINDOWS)
	/* Block till all threads terminate */
	if (WaitForMultipleObjects(nr_thrds, hthrs, TRUE,
	    INFINITE) == WAIT_FAILED) {
		fprintf(stderr, "dispatcher: wait failed %d\n",
		    GetLastError());
	}

	for (i = 0; i < nr_thrds; i++)
		CloseHandle(hthrs[i]);
#if 0
	fprintf(stderr, "dispatcher: ALL threads finished\n");
#endif
#else	/* !_WINDOWS */
	for (i = 0; i < nr_thrds; i++) {
		hctx = &mghttpsw[i];
		pthread_join(hctx->tc_id, NULL);
#if 0
		fprintf(stderr, "dispatcher: thread %d finished\n",
		    hctx->tc_seq);
#endif
	}
#endif

	if (status_thr_disable == 0) {
#if defined (_WINDOWS)
		/* Give watchcat a chance to write correct information. */
		Sleep(100);
		if (TerminateThread(watchcat, 0) == FALSE)
			fprintf(stderr, "terminate_thread: (%d)\n",
			    GetLastError() );
#else
		usleep(100000);
		if (pthread_cancel(watchcat) != 0)
			perror("pthread_cancel");
#endif
	}
	usec = usecs() - usec;
	/* This should be on watch_cat exit */
	printf("\n\n");

	if (bench_time != 0 || debug != 0)
		fprintf(stderr, "finsihed in %lu %s\n",
		    usec < 1000 ? usec/1000 : usec/1000000,
		    usec < 1000 ? "milli-seconds" : "seconds");

	getoutputfile(ofile, sizeof(ofile), http->hr_url->url,
	    http->hr_url->host);
	copy_all(mghttpsw, nr_thrds, ofile);
	return (0);
}

__thread_return
httpfetch_thr(data)
	void *data;
{
	int fd;
	struct thr_context *hctx;

	hctx = (struct thr_context *)data;

#if defined (_WINDOWS)
	hctx->tc_id = GetCurrentThread();
#else
	hctx->tc_id = pthread_self();
#endif

	if (hctx->tc_stat != GT_RUNNING)
		hctx->tc_stat = GT_RUNNING;

	if ((fd = open(hctx->tc_tfil, O_CREAT | O_WRONLY,
	    S_IRUSR | S_IWUSR)) < 0) {
		fprintf(stderr, "httpfetch_thr%d: %s: creat:"
		    " %s\n", hctx->tc_seq, hctx->tc_tfil, strerror(errno) );
		return (THREAD_NULL);
	}
	/* Seek to EOF in case we're resuming previous session */
	lseek(fd, 0, SEEK_END);

#ifdef DEBUG
	if (debug > 0)
		fprintf(stderr, "thread%d: size %d, %d-%d\n",
		    hctx->tc_seq, hctx->tc_totsiz, hctx->tc_offset.start,
		    hctx->tc_offset.end);
#endif

	hctx->tc_ofd = fd;
	if (http_get_r(hctx) < 0)
		herr_done(hctx);
	bclose(fd);

#if defined (_WINDOWS)
	ExitThread(0);
#else
	pthread_exit(NULL);
#endif
	/* NOTREACHED */
	return (THREAD_NULL);
}

__thread_return
status_thr(arg)
	void  *arg;
{
	int i;
	float prec;
	struct thr_context *hctx;
	struct http_request *http;
#if defined (DOWNLOAD_RATE)
	u_long r_time;
	int r_bdiff, rbtot, contlen;
	char r_buf[BUFSIZ];
	char r_perc[BUFSIZ];
#endif

	/* Give other threads a chance to start up */
#if defined (_WINDOWS)
	Sleep(100);
#else	/* !_WINDOWS */
	usleep(100000);
#endif

	for (;;) {
		hctx = &mghttpsw[0];
		http = hctx->tc_httpr;
		printf("%s(%s) ",
		    http->sio.sio_secured != 0 ? "+" : "",
		    http->hr_url->path ? http->hr_url->path : "ROOT");

#if defined (DOWNLOAD_RATE)
		if (conn_speed != 0) {
			r_time = timems();
			for (rbtot = 0, i = 0; i < MAX_GET_THREADS; i++) {
				hctx = &mghttpsw[i];
				if (hctx->tc_stat == GT_RUNNING)
					rbtot += hctx->tc_cursiz;
			}
			r_bdiff = rbtot;
		}
#endif

		for (i = 0; i < MAX_GET_THREADS; i++) {
			hctx = &mghttpsw[i];
			if (hctx->tc_stat == GT_RUNNING) {
				prec = ((float)hctx->tc_cursiz /
				    hctx->tc_totsiz) * (float)100.0;
				printf("%3.0f%% ", prec);
			} else
				printf(" X  ");
		}

#if defined (_WINDOWS)
		Sleep(100);
#else
		usleep(100000);
#endif

#if defined (DOWNLOAD_RATE)
		if (conn_speed != 0) {
			r_time = timems() - r_time;
			for (rbtot = 0, i = 0; i < MAX_GET_THREADS; i++) {
				hctx = &mghttpsw[i];
				if (hctx->tc_stat == GT_RUNNING) {
					rbtot += hctx->tc_cursiz;
					contlen = 
					    hctx->tc_httpr->hdrs.content_length;
				}
			}
			r_bdiff = rbtot - r_bdiff;
			(void) retr_rate(r_bdiff, r_time, r_buf,
			    sizeof(r_buf) );
			printf("%s (%s)", r_buf, 
			    retr_percentage(rbtot, -1, 
			    r_perc, sizeof(r_perc)));
		}
#endif

		printf("\r");
		fflush(stdout);
	}

#if defined (_WINDOWS)
	ExitThread(0);
#else
	pthread_exit(NULL);
#endif
	/* NOTREACHED */
	return (THREAD_NULL);
}

int
prepare_chunks_dir(odir)
	char	*odir;
{

#if defined (_WINDOWS)
	if (mkdir(odir) < 0) {
#else
	if (mkdir(odir, S_IRWXU|S_IRGRP|S_IXGRP) < 0 &&
	    errno != EEXIST) {
#endif
		perror("mkdir");
		return (-1);
	}
	return (0);
}

int
getoutputdir(buf, len, requri, host)
	char	*buf;		/* Buffer to write output */
	int	len;		/* Buffer length */
	char	*requri;
	char	*host;
{
	char *s;

	if (requri != NULL) {
		s = requri + (strlen(requri) - 1);
		do {
			if (*s == '/')
				break;
		} while (s != requri && *(s--));
	} else
		s = "root";
	if (*s == '/')
		s++;
	snprintf(buf, len, ".%s-%s", host, s);
	return 0;
}

int
prepare_chunk_output(hctx, odir, index)
	struct	thr_context *hctx;
	char	*odir;
	int	index;
{

#if defined (_WINDOWS)
	snprintf(hctx->tc_tfil, sizeof(hctx->tc_tfil), "%s\\%s%d",
	    odir, CHUNK_PREFIX, index);
#else
	snprintf(hctx->tc_tfil, sizeof(hctx->tc_tfil), "%s/%s%d",
	    odir, CHUNK_PREFIX, index);
#endif
	return (0);
}

/*
 * Return approperiate threads count based on object size.
 */
int
getnthrds(siz)
{

	if (siz < MINIMUM_SIZE)
		return (1);
	if (siz <= ONE_MEGA)
		return (2);
	if (siz <= ONE_MEGA*2)
		return (4);
	return (MAX_GET_THREADS);
}

int
getchunksiz(nrthrds, siz)
{

	return ((siz/nrthrds));
}

int
inittime(zero)
	struct timeval *zero;
{

#if defined (_WINDOWS)
	zero->tv_usec = GetTickCount();
	return (0);
#else
	return (gettimeofday(zero, NULL) );
#endif
}

long
usecs()
{
	long d;
	struct timeval n;

#if defined (_WINDOWS)
	n.tv_usec = GetTickCount();
	d = (n.tv_usec - zero.tv_usec) * 1000;
#else
	if (gettimeofday(&n, NULL) < 0)
		return (0);
	d = (n.tv_sec - zero.tv_sec) * 1000000;
	d += n.tv_usec - zero.tv_usec;
#endif
	return (d);
}


int
copy_all(hctxsw, nr_thrds, ofile)
	struct thr_context *hctxsw;
	int nr_thrds;
	char *ofile;
{
	int i, ifd, ofd, nrd, nwr, idx;
	struct thr_context *hctx;
	char buffer[4096];
	char tags[] = "\\|/-";

	if ((ofd = creat(ofile, S_IRUSR | S_IWUSR) ) < 0) {
		fprintf(stderr, "copy_all: %s: creat: %s\n",
		    ofile, strerror(errno) );
		return (-1);
	}

	fprintf(stderr, "merging %d chunks ...  ", nr_thrds);

	for (i = 0, idx = 0; i < nr_thrds; i++) {
		hctx = &hctxsw[i];

		if ((ifd = open(hctx->tc_tfil, O_RDONLY, 0) ) < 0) {
			fprintf(stderr, "copy_all: %s: open: %s\n",
			    hctx->tc_tfil, strerror(errno) );
			return (-1);
		}

		for (;; idx++) {
			fprintf(stderr, "\b%c", tags[idx & 0x03]);

			if ((nrd = read(ifd, buffer, sizeof(buffer) ) ) < 0) {
				fprintf(stderr, "copy_all: %s: read: %s\n",
				    hctx->tc_tfil, strerror(errno) );
				break;
			}

			if (nrd == 0) {
				break;
			}

			if ((nwr = write(ofd, buffer, nrd) ) < 0) {
				fprintf(stderr, "copy_all: %s: write: %s\n",
				    hctx->tc_tfil, strerror(errno) );
				break;
			}
		}
		close(ifd);
	}
	fprintf(stderr, "\bdone\n");
	close(ofd);
	close(ifd);
	return (0);
}

/*
 * This routine is called if http_get_r() failed for some reason, and
 * perform approperiate action besed on the reason of failure.
 */
void
herr_done(hctx)
	struct thr_context *hctx;
{

	switch (hctx->tc_retstat) {
	case HTERR_HTTP:
		printf("removing portion %s\n", hctx->tc_tfil);
		unlink(hctx->tc_tfil);
		break;
	case HTERR_INTR:
	case HTERR_RECVIO:
		/* only for multiple connections HTTP fetch */
		bflush(hctx->tc_ofd);
		break;
	case HTERR_CONNECT:
	case HTERR_LOCIO:
	case HTERR_SIZSHRT:
	default:
#if defined (_WINDOWS)
		;
#endif
	}
	return;
}
