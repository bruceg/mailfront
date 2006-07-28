#include <string.h>
#include <stdlib.h>
#include "mailfront.h"
#include "qmtp.h"
#include <iobuf/iobuf.h>
#include <msg/msg.h>

static str respstr;
static unsigned saved_number;
static int was_final;

static int respond_start(unsigned n, int f)
{
  saved_number = n;
  was_final = f;
  if (respstr.len > 0 && respstr.s[respstr.len-1] != LF)
    if (!str_catc(&respstr, LF)) return 0;
  return 1;
}

static int respond_end(void)
{
  if (was_final) {
    char c;
    if (saved_number >= 500) c = 'D';
    else if (saved_number >= 400 || saved_number < 200) c = 'Z';
    else c = 'K';
    obuf_putu(&outbuf, respstr.len+1);
    obuf_putc(&outbuf, ':');
    obuf_putc(&outbuf, c);
    obuf_putstr(&outbuf, &respstr);
    obuf_putc(&outbuf, ',');
    if (!obuf_flush(&outbuf)) return 0;
    respstr.len = 0;
  }
  return 1;
}

static int respond_str(const char* s)
{
  return str_cats(&respstr, s);
}

int respond(unsigned number, int final, const char* msg)
{
  return respond_start(number, final) &&
    respond_str(msg) &&
    respond_end();
}

int respond_resp(const response* resp)
{
  return respond(resp->number, 1, resp->message);
}
