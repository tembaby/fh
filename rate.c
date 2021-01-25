/* $Id: rate.c,v 1.3 2003/07/20 19:04:02 te Exp $ */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#if !defined(min)
# define min(a,b)	((a) <= (b) ? (a) : (b))
#endif

#if defined (UNIX)
# include <sys/time.h>
#endif

#if defined (_WINDOWS)
# include <windows.h>
# define snprintf	_snprintf
#endif

/*
 * calc_rate, wtime_granularity, and retr_rate are taken from
 * wget-1.8.1 sources copyrighted by the GNU GPL.
 */

#define TIMER_GETTIMEOFDAY

char	*retr_rate(long,long,char *,int);
u_long timems(void);
double	calc_rate(long,long,int *);
long	wtimer_granularity(void);

static char *rate_names[] = {"B/s", "KB/s", "MB/s", "GB/s" };
static double prv_dlrate = 0.0;		/* In bytes */

char *
retr_rate(long bytes, long msecs, char *res, int reslen)
{
	int units = 0;
	double dlrate;

	dlrate = calc_rate(bytes, msecs, &units);
	snprintf(res, reslen, "%7.2f %s", dlrate, rate_names[units]);
	return (res);
}

/* Calculate the download rate and trim it as appropriate for the
   speed.  Appropriate means that if rate is greater than 1K/s,
   kilobytes are used, and if rate is greater than 1MB/s, megabytes
   are used.

   UNITS is zero for B/s, one for KB/s, two for MB/s, and three for
   GB/s.  */
double
calc_rate(long bytes, long msecs, int *units)
{
	double dlrate;

	if (msecs <= 0)
		/* If elapsed time is 0, it means we're under the granularity of
		   the timer.  This often happens on systems that use time() for
		   the timer.  */
		msecs = wtimer_granularity();

	/*
	 * On conjested network, if OS buffers multiple incoming packets
	 * then (theoritcally) our next read will get large chunks of data
	 * in very low `msecs' (sometimes 0) which will make calculated rate
	 * jumps very high on low bandwidth connections (e.g., 3.0 MB/s on PPP)
	 * (for example 4096 bytes in 0 msecs).
	 *
	 * The next adjustment scheme will insure that the download rate will
	 * never jump too high, but will increase slowly.
	 */
	dlrate = (double)1000 * bytes / msecs;
	if (prv_dlrate != 0) {
		if (dlrate > prv_dlrate) {
			dlrate = min(dlrate, prv_dlrate * 1.5);
		} 
	}
	prv_dlrate = dlrate;
	if (dlrate < 1024.0)
		*units = 0;
	else if (dlrate < 1024.0 * 1024.0)
		*units = 1, dlrate /= 1024.0;
	else if (dlrate < 1024.0 * 1024.0 * 1024.0)
		*units = 2, dlrate /= (1024.0 * 1024.0);
	else
		/* Maybe someone will need this one day.  More realistically, it
		   will get tickled by buggy timers. */
		*units = 3, dlrate /= (1024.0 * 1024.0 * 1024.0);
	return (dlrate);
}

/* Return the assessed granularity of the timer implementation.  This
   is important for certain code that tries to deal with "zero" time
   intervals.  */
long
wtimer_granularity (void)
{
#ifdef TIMER_GETTIMEOFDAY
	/* Granularity of gettimeofday is hugely architecture-dependent.
	   However, it appears that on modern machines it is better than
	   1ms.  */
	return (1);
#endif

#ifdef TIMER_TIME
	/* This is clear. */
	return (1000);
#endif

#ifdef TIMER_WINDOWS
	/* ? */
	return (1);
#endif
}

u_long
timems()
{

#if defined (_WINDOWS)
	return (GetTickCount() );
#endif

#if defined (UNIX)
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		fprintf(stderr, "timems: %s\n", strerror(errno) );
		return (0);
	}

	return (((tv.tv_sec * 1000000) + tv.tv_usec) / 1000);
#endif
	return (0);
}

static char *
bytes2xxx(sz)
	int	sz;
{
	static char b[256];

	if (sz < 1024)
		snprintf(b, sizeof(b), "%d K", sz);
	else if (sz < 1024 * 1024)
		snprintf(b, sizeof(b), "%d KB", sz / 1024);
	else if (sz < 1024 * 1024 * 1024)
		snprintf(b, sizeof(b), "%d MB", sz / (1024 * 1024));
	else
		snprintf(b, sizeof(b), "%d GB", sz / (1024 * 1024 * 1024));
	return (b);
}

char *
retr_percentage(cursz, totsz, res, reslen)
	int	cursz;
	int	totsz;
	char	*res;
	int	reslen;
{
	int perc;

	if (totsz == -1) {
		snprintf(res, reslen, "%s of data", bytes2xxx(cursz));
		return (res);
	}

	if (cursz >= totsz) {
		snprintf(res, reslen, "100%% of %s", bytes2xxx(totsz));
		return (res);
	}

	perc = ((float)cursz / totsz) * 100;
	snprintf(res, reslen, "%d%% of %s", perc, bytes2xxx(totsz));
	return (res);
}

#ifdef __TEST__
long
getbytes()
{
	static int bytes = 0;
	static int flip = 0;

#if defined (_WINDOWS)
	Sleep(100);
#endif

#if defined (UNIX)
	usleep(100000);
#endif

	flip++;
	if (flip % 2)
		bytes = 300;
	else
		bytes = 350;
	return (bytes);
}

volatile long size = 0;

int
main()
{
	int i;
	unsigned long rtime, r_starttime;
	long lbytes;
	char buffer[512];

	lbytes = 0;

	for (i = 0; i < 1000; i++) {
		r_starttime = timems();		/* time before fetching */
		lbytes = size;			/* size before fetching */

		size += getbytes();		/* Fetch routine */

		lbytes = size - lbytes;		/* size differnce after fetching */
		rtime = timems() - r_starttime; /* time in (ms) after fetching */

		(void) retr_rate(lbytes, rtime, buffer, sizeof(buffer) );
		printf("rate %s\r", buffer);
		fflush(stdout);
	}
	printf("\n");
	return (0);
}
#endif /* __TEST__ */
