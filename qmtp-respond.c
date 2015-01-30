#include <string.h>
#include "responses.h"
#include "qmtp.h"
#include <bglibs/iobuf.h>
#include <bglibs/str.h>

int qmtp_respond_line(unsigned num, int final,
		      const char* msg, unsigned long len)
{
  static str resp;
  char c;
  if (resp.len > 0)
    if (!str_catc(&resp, '\n')) return 0;
  if (!str_catb(&resp, msg, len)) return 0;
  if (final) {
    c = (num >= 500)
      ? 'D'
      : (num >= 400 || num < 200)
      ? 'Z'
      : 'K';
    if (!obuf_putu(&outbuf, resp.len + 1)) return 0;
    if (!obuf_putc(&outbuf, ':')) return 0;
    if (!obuf_putc(&outbuf, c)) return 0;
    if (!obuf_write(&outbuf, resp.s, resp.len)) return 0;
    if (!obuf_putc(&outbuf, ',')) return 0;
    resp.len = 0;
  }
  return 1;
}
