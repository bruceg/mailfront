#include <string.h>
#include <stdlib.h>
#include "mailfront.h"
#include "smtp.h"
#include <iobuf/iobuf.h>
#include <msg/msg.h>

const int msg_show_pid = 1;

int respond_start(unsigned number, int final)
{
  return obuf_putu(&outbuf, number) &&
    obuf_putc(&outbuf, final ? ' ' : '-');
}

int respond_end(void)
{
  return obuf_putsflush(&outbuf, CRLF);
}

int respond_str(const char* s)
{
  return obuf_puts(&outbuf, s);
}

int respond(unsigned number, int final, const char* msg)
{
  return respond_start(number, final) &&
    respond_str(msg) &&
    respond_end();
}

static int respond_b(unsigned number, int final,
		     const char* msg, long len)
{
  return respond_start(number, final) &&
    obuf_write(&outbuf, msg, len) &&
    respond_end();
}

int respond_resp(const response* resp, int final)
{
  const char* nl;
  const char* start;
  for (start = nl = resp->message; (nl=strchr(start, '\n')) != 0; start = nl+1)
    if (!respond_b(resp->number, 0, start, nl-start))
      return 0;
  return respond(resp->number, final, start);
}
