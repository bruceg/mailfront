#include <string.h>
#include <stdlib.h>
#include "mailfront.h"
#include "smtp.h"
#include "iobuf/iobuf.h"

int respond_start(unsigned number, int final)
{
  return obuf_putu(&outbuf, number) &&
    obuf_putc(&outbuf, final ? ' ' : '-');
}

int respond_end(void)
{
  return obuf_putc(&outbuf, CR) &&
    obuf_putc(&outbuf, LF) &&
    obuf_flush(&outbuf);
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
  if (resp->number >= 400) log1(resp->message);
  return respond(resp->number, final, resp->message);
}
