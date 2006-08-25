#include <string.h>
#include <stdlib.h>
#include "mailfront.h"
#include "smtp.h"
#include <iobuf/iobuf.h>
#include <msg/msg.h>

static str respstr;

static int respond_start(unsigned number, int final)
{
  return str_truncate(&respstr, 0) &&
    str_catu(&respstr, number) &&
    str_catc(&respstr, final ? ' ' : '-');
}

static int respond_end(void)
{
  if (respstr.s[0] >= '4')
    msg1(respstr.s);
  return obuf_putstr(&outbuf, &respstr) &&
    obuf_puts(&outbuf, CRLF);
}

static int respond_b(unsigned number, int final,
		     const char* msg, long len)
{
  return respond_start(number, final) &&
    str_catb(&respstr, msg, len) &&
    respond_end();
}

int smtp_respond(const response* resp)
{
  const char* msg;
  const char* nl;
  for (msg = resp->message; (nl = strchr(msg, '\n')) != 0; msg = nl + 1)
    if (!respond_b(resp->number, 0, msg, nl-msg))
      return 0;
  return respond_start(resp->number, 1)
    && str_cats(&respstr, msg)
    && respond_end()
    && obuf_flush(&outbuf);
}
