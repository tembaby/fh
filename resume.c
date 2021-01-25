/* $Id: resume.c,v 1.3 2002/09/09 19:13:21 te Exp $ */

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
#include <errno.h>
#include <fcntl.h>
#if defined (HAVE_REGEX)
# include <regex.h>
#endif	/* HAVE_REGEX */
#if defined (UNIX)
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <http.h>
#include <fh.h>

#define RESUME_FILE	".download"
#ifdef DEBUG
# define DEBUG_RSUM(s) do { \
	/*if (debug > 0) */\
		printf s; \
} while (0)
#else
# define DEBUG_RSUM(s)
#endif

#define INIT_RSUM_OFFSETS(rsum) do { \
	register int i; \
	for (i = 0; i < (rsum)->rs_thrcnt; i++) \
		(rsum)->rs_ranges[i].cstart = 0; \
} while (0)

#define RE_THRS_STRING	"(thread)([0-9])+(-range)([ \t])*="

#if defined (HAVE_REGEX)
static int re_up = 0;
static regex_t *re_thrs = NULL;
#endif	/* HAVE_REGEX */

static int resume_init(void);
static void resume_deinit(void);
static int resume_thrs_match(char *,char **,char **,char **);
#if defined (_WINDOWS)
static int _match_thr_line(char *,char **,char **,char **);
#endif
static void resume_parse(char *,struct resume *);
static void resume_offsets(struct resume *);

int
resume_save(rsum)
	struct	resume *rsum;
{
	int fd, i;
	char rfil[PATH_MAX];
	char buf[BUFSIZ];

#if defined (_WINDOWS)
	snprintf(rfil, sizeof(rfil), "%s\\%s", rsum->rs_dir, RESUME_FILE);
#else
	snprintf(rfil, sizeof(rfil), "%s/%s", rsum->rs_dir, RESUME_FILE);
#endif

	if ((fd = open(rfil, O_WRONLY | O_CREAT | O_EXCL,
	    S_IRUSR | S_IWUSR) ) < 0) {
		fprintf(stderr, "resume_save(%s): %s\n",
		    rfil, strerror(errno));
		return (-1);
	}

#define RESUME_WRITE(buf, buflen) do { \
	if (write(fd, buf, strlen(buf) ) < 0) { \
		fprintf(stderr, "resume_save: write(%s): %s\n", \
		    buf, strerror(errno)); \
		close(fd); \
		return (-1); \
	} \
} while (0)

	/* Will trancate URL if it's too long */
	snprintf(buf, sizeof(buf), "url=%s\n", rsum->rs_url);
	RESUME_WRITE(buf, strlen(buf));

	snprintf(buf, sizeof(buf), "object-size=%lu\n", rsum->rs_size);
	RESUME_WRITE(buf, strlen(buf));

	snprintf(buf, sizeof(buf), "threads-count=%d\n", rsum->rs_thrcnt);
	RESUME_WRITE(buf, strlen(buf));

	for (i = 0; i < rsum->rs_thrcnt; i++) {
		snprintf(buf, sizeof(buf), "thread%d-range=%d-%d\n",
		    i, rsum->rs_ranges[i].start, rsum->rs_ranges[i].end);
		RESUME_WRITE(buf, strlen(buf));
	}
	close(fd);
	return (0);
}

int
resume_read(rsum)
	struct	resume *rsum;
{
	FILE *f;
	char *line;
	char rfil[PATH_MAX];

	INIT_RSUM_OFFSETS(rsum);

#if defined (_WINDOWS)
	snprintf(rfil, sizeof(rfil), "%s\\%s", rsum->rs_dir, RESUME_FILE);
#else
	snprintf(rfil, sizeof(rfil), "%s/%s", rsum->rs_dir, RESUME_FILE);
#endif

	if ((f = fopen(rfil, "r") ) == NULL)
		return (-1);

	resume_init();

#ifdef DEBUG
	if (debug > 0)
		printf("resume_read: loading resume information from %s\n",
		    rfil);
#endif

	while ((line = getline(f)) != NULL) {
		resume_parse(line, rsum);
		free(line);
	}
	fclose(f);
	resume_offsets(rsum);
	resume_deinit();
	return (0);
}

#define SKIP_LWS(p) do { \
	while (*(p) != '\0' && (*(p) == ' ' || *(p) == '\t')) \
		p++; \
} while (0)

static void
resume_parse(line, rsum)
	char	*line;
	struct	resume *rsum;
{
	int index;
	char *p, *s;
	char *sp, *ep, *num;

	p = line;
	SKIP_LWS(p);

#define SKIP_TO_VALUE(p, len) do { \
		p += len; \
		SKIP_LWS(p); \
		if (*(p) != '=') { \
			fprintf(stderr, "resume_parse: malformed line:\n" \
			    "%s\n", line); \
			for (s = line; s != p; s++) \
				printf(" "); \
			printf("^\n"); \
			return; \
		} \
		(p)++; \
		SKIP_LWS(p); \
} while (0)

	if (strncmp(p, "url", 3) == 0) {
		SKIP_TO_VALUE(p, 3);
		rsum->rs_url = strdup(p);
		DEBUG_RSUM(("url = %s\n", rsum->rs_url));
		return;
	}
	if (strncmp(p, "object-size", 11) == 0) {
		SKIP_TO_VALUE(p, 11);
		rsum->rs_size = strtoul(p, NULL, 10);
		DEBUG_RSUM(("object-size = %lu\n", rsum->rs_size));
		return;
	}
	if (strncmp(p, "threads-count", 13) == 0) {
		SKIP_TO_VALUE(p, 13);
		rsum->rs_thrcnt = atoi(p);
		DEBUG_RSUM(("threads-count = %d\n", rsum->rs_thrcnt));
		return;
	}
	if (resume_thrs_match(p, &sp, &ep, &num) == 0) {
		index = atoi(num);
		if (index < 0 || index > MAX_GET_THREADS) {
			fprintf(stderr, "resume_parse: invalid thread value: "
			    "%d\n", index);
			return;
		}
		p = ep;
		SKIP_LWS(p);
		rsum->rs_ranges[index].start = strtoul(p, NULL, 10);
		while (*p != '\0' && *p != '-')
			p++;
		if (*p == '\0')
			return;
		p++;
		rsum->rs_ranges[index].end = strtoul(p, NULL, 10);
		DEBUG_RSUM(("thread (%d): ranges = %d-%d\n", index,
		    rsum->rs_ranges[index].start,
		    rsum->rs_ranges[index].end));
		return;
	}
	return;
}

static int
resume_init()
{

#if defined (HAVE_REGEX)
	if (re_up != 0)
		return (0);
	if ((re_thrs = malloc(sizeof(regex_t))) == NULL) {
		fprintf(stderr, "resume_init: out of dynamic memory\n");
		return (-1);
	}
	if (regcomp(re_thrs, RE_THRS_STRING, REG_EXTENDED) != 0) {
		fprintf(stderr, "resume_init: invalid pattern: %s\n",
		    RE_THRS_STRING);
		return (-1);
	}
	re_up++;
#endif	/* HAVE_REGEX */
	return (0);
}

static void
resume_deinit()
{

#if defined (HAVE_REGEX)
	if (re_up != 0)
		regfree(re_thrs);
#endif	/* HAVE_REGEX */
	return;
}

/*
 * Match the givin line against the pattern string RE_THRS_STRING,
 * returning 0 if match was found and pointing `sp' and `ep' to the
 * start and the end of the matched string respectiviley.
 * Or -1 if no match was found.
 */
static int
resume_thrs_match(line, sp, ep, num)
	char	*line;		/* The line we're matching pattern to */
	char	**sp;		/* The start of the pattern matched */
	char	**ep;		/* The end of the pattern matched */
	char	**num;		/* Pointer to numeric string in expr */
{
#if defined (HAVE_REGEX)
	regmatch_t rm;

	if (regexec(re_thrs, line, 1, &rm, 0) != 0)
		return (-1);
	*sp = line + rm.rm_so;
	*ep = line + rm.rm_eo;
	*num = line + strlen("thread");
	return (0);
#else
	return (_match_thr_line(line, sp, ep, num));
#endif
}

static void
resume_offsets(rsum)
	struct	resume *rsum;
{
	int index;
	struct stat s;
	char file[PATH_MAX];

	for (index = 0; index < rsum->rs_thrcnt; index++) {
		rsum->rs_ranges[index].cstart = 0;
#if defined (_WINDOWS)
		snprintf(file, sizeof(file), "%s\\%s%d", rsum->rs_dir,
		    CHUNK_PREFIX, index);
#else	/* !_WINDOWS */
		snprintf(file, sizeof(file), "%s/%s%d", rsum->rs_dir,
		    CHUNK_PREFIX, index);
#endif
		if (stat(file, &s) < 0)
#if defined (DEVEL)
			goto _cont;
#else	/* !DEVEL */
			continue;
#endif
		rsum->rs_ranges[index].cstart = s.st_size;
#if defined (DEVEL)
_cont:
		printf("resume_offsets: thread(%d): restarting from %d\n",
		    index, rsum->rs_ranges[index].cstart);
#endif
	}
	return;
}

/*
 * Decide if the resume information calculated from HTTP request
 * is consistent with the one read from file.
 */
int
resume_consistent(rp)
	struct	resume_pair *rp;
{
	int i;

	if (RS_READ(rp)->rs_size != RS_SAVE(rp)->rs_size)
		return (-1);
	if (RS_READ(rp)->rs_thrcnt != RS_SAVE(rp)->rs_thrcnt) {
		RS_SAVE(rp)->rs_thrcnt = RS_READ(rp)->rs_thrcnt;
#if 0
		return (-1);
#endif
	}
	for (i = 0; i < RS_READ(rp)->rs_thrcnt; i++) {
		if (RS_READ(rp)->rs_ranges[i].start !=
		    RS_SAVE(rp)->rs_ranges[i].start ||
		    RS_READ(rp)->rs_ranges[i].end !=
		    RS_SAVE(rp)->rs_ranges[i].end)
			return (-1);
	}
	return (0);
}

void
resume_dprint(rp)
	struct	resume_pair *rp;
{
	int i;

	printf("save: (sz=%d, thrs=%d)\n", RS_SAVE(rp)->rs_size,
	    RS_SAVE(rp)->rs_thrcnt);
	printf("read: (sz=%d, thrs=%d)\n", RS_READ(rp)->rs_size,
	    RS_READ(rp)->rs_thrcnt);
	printf("save: ");
	for (i = 0; i < RS_SAVE(rp)->rs_thrcnt; i++)
		printf("%d/%d ", RS_SAVE(rp)->rs_ranges[i].start,
		    RS_SAVE(rp)->rs_ranges[i].end);
	printf("\n");
	printf("read: ");
	for (i = 0; i < RS_READ(rp)->rs_thrcnt; i++)
		printf("%d/%d ", RS_READ(rp)->rs_ranges[i].start,
		    RS_READ(rp)->rs_ranges[i].end);
	printf("\n");
	return;
}

#if !defined (HAVE_REGEX)
static int
_match_thr_line(line, sp, ep, num)
	char	*line;		/* The line we're matching pattern to */
	char	**sp;		/* The start of the pattern matched */
	char	**ep;		/* The end of the pattern matched */
	char	**num;		/* Pointer to numeric string in expr */
{
	char *p;

	p = line;
	SKIP_LWS(p);
	if (strncmp(p, "thread", 6) != 0)
		return (-1);
	*num = p + 6;
	p += 6;
	if (isdigit(*p) != 0)
		return (-1);
	while (isdigit(*p) != 0)
		p++;
	if (*p++ != '-')
		return (-1);
	if (strncmp(p, "range", 5) != 0)
		return (-1);
	p += 5;
	SKIP_LWS(p);
	if (*p++ != '=')
		return (-1);
	SKIP_LWS(p);
	*sp = p;
	while (isdigit(*p) != 0)
		p++;
	if (*p++ != '-')
		return (-1);
	*ep = p;
	return (0);
}
#endif	/* !HAVE_REGEX */
