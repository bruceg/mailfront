#include <string.h>
#include <stdlib.h>
#include "mailfront.h"
#include "qmtp.h"
#include <iobuf/iobuf.h>
#include <msg/msg.h>

const int msg_show_pid = 1;

static str respstr;
static unsigned number;
static int was_final;

int respond_start(unsigned n, int f)
{
  number = n;
  was_final = f;
  return 1;
}

int respond_end(void)
{
  if (was_final) {
    char c;
    if (number >= 500) c = 'D';
    else if (number >= 400 || number < 200) c = 'Z';
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

int respond_str(const char* str)
{
  return str_cats(&respstr, str);
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
