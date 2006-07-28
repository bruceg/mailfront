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
    obuf_putsflush(&outbuf, CRLF);
}

static int respond_str(const char* s)
{
  return str_cats(&respstr, s);
}

static int respond_b(unsigned number, int final,
		     const char* msg, long len)
{
  return respond_start(number, final) &&
    str_catb(&respstr, msg, len) &&
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
