#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <systime.h>
#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"
#include "qmtp.h"

static const response* resp;

static char buf[8192];
static str line;

static void die(const char* msg)
{
  response r = { 451, msg };
  respond(&r);
  exit(111);
}

static void get_wrapper(ibuf* in)
{
  unsigned long wraplen;
  switch (get_netstring_len(in, &wraplen)) {
  case -1: exit(0);
  case 0: die("Invalid wrapper netstring");
  }
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
  char ch;
  while (ibuf_peek(in, &ch)) {
    if (ch == ',') return;
    switch (get_netstring(in, &line)) {
    case -1: die("EOF while reading recipient list");
    case 0: die("Invalid recipient netstring");
    }
    msg3("recipient <", line.s, ">");
    if (response_ok(resp))
      resp = handle_recipient(&line, 0);
  }
  die("EOF before end of recipient list");
}

static void get_package(ibuf* in)
{
  resp = handle_reset();
  get_wrapper(in);
  get_body(in);
  get_sender(in);
  get_recips(in);
  if (response_ok(resp))
    resp = handle_message_end();
  if (!resp) resp = &resp_accepted;
  if (!respond(resp)) die("EOF while sending response");
}

static int mainloop(const struct command* commands)
{
  alarm(3600);
  get_package(&inbuf);
  return 0;
  (void)commands;
}

struct protocol protocol = {
  .version = PROTOCOL_VERSION,
  .name = "QMQP",
  .respond_line = qmtp_respond_line,
  .mainloop = mainloop,
};
