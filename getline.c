/* $Id: getline.c,v 1.1 2002/05/10 02:40:05 te Exp $ */

/* Tamer Embaby <tsemba@menanet.net> */

/* 
 * Reads a line from a file and returns it to the calling subroutine, 
 * _regardless of it's length_. 
 *
 * Tue Nov 23 10:57:26 EET 1999, Fixed the problem when it ignore last line if
 * it doesn't contain '\n' + some enhancements.
 *
 * It returns malloc()'d buffer, caller must free after use.
 *
 * This code is FREE as long as this comment and the above
 * remain intact.
 * 
 * TODO: Buffered I/O.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define START_LEN 	256

char * getline(FILE * file)
{
	char * buf;
	char * p = 0;
	size_t len, new_len;
	int n = 0;
	char c;
	short dirty_buffer = 0;

	len = START_LEN;
	buf = (char *)malloc(START_LEN + 1);
	if (!buf) {
#ifdef DEBUG
		perror("malloc()");
#endif
		return NULL;
	}

	p = buf;

	do {
		c = getc(file);
		if (c == EOF) {
			if (dirty_buffer)
				break;
			return NULL;
		}
		if (c == '\n')
			break;

		if (n++ >= len) {
			new_len = len << 1;
			buf = (char *)realloc(buf, new_len + 1);
			if (buf == NULL) {
#ifdef DEBUG
				perror("realloc()");
#endif
	                        return NULL;
			}

			len = new_len;
			p = buf;
	                p += len >> 1;
		 }
		 *p = c;
		 p++;
		 dirty_buffer = 1;
	  } while( 1 );

	  *p = '\0';

	  return buf;
}

#if 0
int
main()
{
  char* lbuf = 0;
  FILE* file;

  file = fopen("./test", "r");

  while ( 1 ) {
	  lbuf = getline(file);
	  if (lbuf == NULL)
	  	  break;
	  printf("%s\n", lbuf);
	  free(lbuf);
  }

  fclose(file);

  return 0;
}
#endif
