#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <bglibs/systime.h>
#include <bglibs/iobuf.h>
#include <bglibs/msg.h>
#include <bglibs/str.h>

#include "mailfront.h"
#include "qmtp.h"

static const response* resp;

static char buf[8192];
static str line;
static str tmp;

static void die(const char* msg) __NORETURN__;
static void die(const char* msg)
{
  response r = { 451, msg };
  respond(&r);
  exit(111);
}

static void get_body(ibuf* in)
{
  unsigned long bodylen;
  char nl;
  switch (get_netstring_len(in, &bodylen)) {
  case -1: exit(0);
  case 0: die("Invalid message body netstring");
  }
  if (bodylen == 0) die("Zero length message");
  --bodylen;
  if (!ibuf_getc(in, &nl)) die("EOF while reading body NL");
  if (nl != LF) die("Cannot handle non-LF line terminator");
  if (response_ok(resp))
    resp = handle_data_start();
  while (bodylen > 0) {
    unsigned long len = sizeof buf;
    if (len > bodylen) len = bodylen;
    if (!ibuf_read(in, buf, len) && in->count == 0)
      die("EOF while reading body");
    if (response_ok(resp))
      handle_data_bytes(buf, in->count);
    bodylen -= in->count;
  }
  if (!ibuf_getc(in, &nl)) die("EOF while reading comma");
  if (nl != ',') die("Invalid netstring terminator");
}

static void get_sender(ibuf* in)
{
  switch (get_netstring(in, &line)) {
  case -1: die("EOF while reading sender address");
  case 0: die("Invalid sender netstring");
  }
  msg3("sender <", line.s, ">");
  if (response_ok(resp))
    resp = handle_sender(&line, 0);
}

static void get_recips(ibuf* in)
{
  unsigned long i;
  switch (get_netstring(in, &line)) {
  case -1: die("EOF while reading recipient list");
  case 0: die("Invalid recipient netstring");
  }
  for (i = 0; response_ok(resp) && i < line.len; i++) {
    unsigned long j;
    unsigned long len;
    for (len = 0, j = i; j < line.len; ++j) {
      if (line.s[j] == ':') break;
      if (line.s[j] < '0' || line.s[j] > '9')
	die("Invalid netstring length");
      len = len * 10 + line.s[j] - '0';
    }
    ++j;
    if (j + len > line.len) die("Netstring length too long");
    if (line.s[j+len] != ',') die("Netstring missing comma");
    str_copyb(&tmp, line.s+j, len);
    msg3("recipient <", tmp.s, ">");
    if (response_ok(resp))
      resp = handle_recipient(&tmp, 0);
    i = j + len;
  }
}

static void get_package(ibuf* in)
{
  resp = handle_reset();
  get_body(in);
  get_sender(in);
  get_recips(in);
  if (response_ok(resp))
    resp = handle_message_end();
  if (!resp) resp = &resp_accepted_message;
  if (!respond(resp)) die("EOF while sending response");
}

static int mainloop(const struct command* commands)
{
  alarm(3600);
  for (;;)
    get_package(&inbuf);
  return 0;
  (void)commands;
}

struct protocol protocol = {
  .version = PROTOCOL_VERSION,
  .name = "QMTP",
  .respond_line = qmtp_respond_line,
  .mainloop = mainloop,
};
