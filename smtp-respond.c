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

static int respond_b(unsigned number, int final,
		     const char* msg, long len)
{
  return respond_start(number, final) &&
    obuf_write(&outbuf, msg, len) &&
    respond_end();
}

int respond(unsigned number, int final, const char* msg)
{
  const char* nl;
  while ((nl = strchr(msg, '\n')) != 0) {
    if (!respond_b(number, 0, msg, nl-msg))
      return 0;
    msg = nl + 1;
  }
  return respond_start(number, final) &&
    respond_str(msg) &&
    respond_end();
}

int respond_resp(const response* resp, int final)
{
  return respond(resp->number, final, resp->message);
}
