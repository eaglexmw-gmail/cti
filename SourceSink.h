#ifndef _SOURCESINK_H_
#define _SOURCESINK_H_

#include "Array.h"
#include <stdint.h>
#include <stdio.h>		/* FILE * */
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct {
  FILE *f;			/* file */
  FILE *p;			/* pipe (popen) */
  int s;			/* socket */
  struct sockaddr_in addr;	/* socket details... */
  socklen_t addrlen;		/* socket details... */
  ArrayU8 * extra;		/* extra data leftover after certain operations */
} IO_common;

typedef struct {
  IO_common io;
  long file_size;
  int eof;
  int eof_flagged;
  int persist;			/* return on EOF */
  uint64_t bytes_read;
  time_t t0;
} Source;

extern Source *Source_new(char *name /* , options? */ );
extern ArrayU8 * Source_read(Source *source);
extern int Source_poll_read(Source *source, int timeout);
extern int Source_seek(Source *source, long amount);
extern int Source_set_offset(Source *source, long amount);
extern long Source_tell(Source *source);
extern void Source_acquire_data(Source *source, ArrayU8 *chunk, int *needData);
extern void Source_set_persist(Source *source, int value);
extern void Source_close_current(Source *source);
extern void Source_free(Source **source);


typedef struct {
  IO_common io;
} Sink;

extern Sink *Sink_new(char *name);
extern void Sink_write(Sink *sink, void *data, int length);
extern void Sink_flush(Sink *sink);
extern void Sink_close_current(Sink *sink);
extern void Sink_free(Sink **sink);


typedef struct {
  /* 2-way socket or pipe. */
  IO_common io;
  unsigned int read_timeout_ms;
} Comm;

extern Comm * Comm_new(char * name);
extern void Comm_close(Comm * comm);
extern void Comm_write_string_with_zero(Comm * comm, String *str);
extern String * Comm_read_string_to_zero(Comm * comm);

#if 0
extern void Comm_read_append_array(Comm * comm, Array_u8 * array);
extern void Comm_write_from_array(Comm * comm,  Array_u8 * array, unsigned int * offset);
// write up to 32000 bytes, maybe less, starting from offset, offset is updated
extern void Comm_write_from_array_complete(Comm * comm,  Array_u8 * array);
#endif
extern void Comm_free(Comm **comm);


#endif
