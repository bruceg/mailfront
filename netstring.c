#include <iobuf/iobuf.h>
#include <str/str.h>
#include "mailfront.h"

int get_netstring_len(ibuf* in, unsigned long* i)
     /* Returns: 1 good number, 0 format error, -1 EOF */
{
  char c;
  *i = 0;
  for (*i = 0; ibuf_getc(in, &c); *i = (*i * 10) + (c - '0')) {
    if (c == ':') return 1;
    if (c < '0' || c > '9') return 0;
  }
  return -1;
}

int get_netstring(ibuf* in, str* s)
{
  unsigned long len;
  char ch;
  switch (get_netstring_len(in, &len)) {
  case -1: return -1;
  case 0: return 0;
  }
  if (!str_ready(s, len)) return -1;
  s->s[len] = 0;
  if (!ibuf_read(in, s->s, len)) return -1;
  s->len = len;
  if (!ibuf_getc(in, &ch)) return -1;
  return ch == ',';
}
