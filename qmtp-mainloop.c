#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <systime.h>
#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"
#include "qmtp.h"

unsigned long maxdatabytes;

static const response resp_accepted = {0,250,"Message accepted"};

static const response* resp;

static char buf[8192];
static str line;
static str tmp;

static int get_netstring_len(ibuf* in, unsigned long* i)
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

static int get_netstring(ibuf* in, str* s)
{
  unsigned long len;
  char ch;
  switch (get_netstring_len(in, &len)) {
  case -1: return -1;
  case 0: return 0;
  }
  if (!str_ready(s, len)) return -1;
  if (!ibuf_read(in, s->s, len)) return -1;
  s->len = len;
  if (!ibuf_getc(in, &ch)) return -1;
  return ch == ',';
}

static void get_body(ibuf* in)
{
  unsigned long bodylen;
  char nl;
  switch (get_netstring_len(in, &bodylen)) {
  case -1: exit(0);
  case 0: die1(111, "Invalid message body netstring");
  }
  if (bodylen == 0) die1(111, "Zero length body");
  --bodylen;
  if (!ibuf_getc(in, &nl)) die1(111, "EOF while reading body NL");
  if (nl != LF) die1(111, "Cannot handle non-LF line terminator");
  resp = std_handle_data_start(0, "QMTP");
  while (bodylen > 0) {
    unsigned long len = sizeof buf;
    if (len > bodylen) len = bodylen;
    if (!ibuf_read(in, buf, len) && in->count == 0)
      die1(111, "EOF while reading body");
    if (response_ok(resp)) std_handle_data_bytes(buf, in->count);
    bodylen -= in->count;
  }
  if (!ibuf_getc(in, &nl)) die1(111, "EOF while reading comma");
  if (nl != ',') die1(111, "Invalid netstring terminator");
}

static void get_sender(ibuf* in)
{
  switch (get_netstring(in, &line)) {
  case -1: die1(111, "EOF while reading sender address");
  case 0: die1(111, "Invalid sender netstring");
  }
  if (response_ok(resp))
    resp = std_handle_sender(&line);
}

static void get_recips(ibuf* in)
{
  unsigned long i;
  switch (get_netstring(in, &line)) {
  case -1: die1(111, "EOF while reading recipient list");
  case 0: die1(111, "Invalid recipient netstring");
  }
  for (i = 0; response_ok(resp) && i < line.len; i++) {
    unsigned long j;
    unsigned long len;
    for (len = 0, j = i; j < line.len; ++j) {
      if (line.s[j] == ':') break;
      if (line.s[j] < '0' || line.s[j] > '9')
	die1(111, "Invalid netstring length");
      len = len * 10 + line.s[j] - '0';
    }
    ++j;
    if (j + len > line.len) die1(111, "Netstring length too long");
    if (line.s[j+len] != ',') die1(111, "Netstring missing comma");
    str_copyb(&tmp, line.s+j, len);
    resp = std_handle_recipient(&tmp);
    i = j + len;
  }
}

static void get_package(ibuf* in)
{
  handle_reset();
  get_body(in);
  get_sender(in);
  get_recips(in);
  if (response_ok(resp)) resp = std_handle_data_end();
  if (!resp) resp = &resp_accepted;
  if (!respond_resp(resp, 1)) die1(111, "EOF while sending response");
}

int qmtp_mainloop(void)
{
  const response* resp;
  if ((resp = std_handle_init()) != 0) {
    respond_resp(resp, 1);
    return 1;
  }
  alarm(3600);
  for (;;) get_package(&inbuf);
}
