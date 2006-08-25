#include <string.h>
#include "responses.h"
#include "qmtp.h"
#include <iobuf/iobuf.h>

int qmtp_respond(const response* resp)
{
  char c;
  long len;
  c = (resp->number >= 500)
    ? 'D'
    : (resp->number >= 400 || resp->number < 200)
    ? 'Z'
    : 'K';
  len = strlen(resp->message);
  obuf_putu(&outbuf, len+1);
  obuf_putc(&outbuf, ':');
  obuf_putc(&outbuf, c);
  obuf_write(&outbuf, resp->message, len);
  obuf_putc(&outbuf, ',');
  if (!obuf_flush(&outbuf)) return 0;
  return 1;
}
