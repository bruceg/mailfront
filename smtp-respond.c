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

int respond_str(const char* str)
{
  return obuf_puts(&outbuf, str);
}

int respond(unsigned number, int final, const char* msg)
{
  return respond_start(number, final) &&
    respond_str(msg) &&
    respond_end();
}

int respond_resp(const response* resp, int final)
{
  if (resp->prev && !respond_resp(resp->prev, 0)) return 0;
  if (resp->number >= 400) msg1(resp->message);
  return respond(resp->number, final, resp->message);
}
