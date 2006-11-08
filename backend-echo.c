#include <string.h>
#include <msg/msg.h>
#include "mailfront.h"

static response resp = { 250, 0 };
static str tmp;

static unsigned long databytes = 0;

static const response* reset(void)
{
  databytes = 0;
  return 0;
}

static const response* sender(str* s)
{
  str_copys(&tmp, "Sender='");
  str_cat(&tmp, s);
  str_cats(&tmp, "'.");
  resp.message = tmp.s;
  return &resp;
}

static const response* recipient(str* r)
{
  str_copys(&tmp, "Recipient='");
  str_cat(&tmp, r);
  str_cats(&tmp, "'.");
  resp.message = tmp.s;
  return &resp;
}

static const response* data_block(const char* bytes, unsigned long len)
{
  databytes += len;
  if (databytes == len) {
    /* First line is always Received, log the first two lines. */
    const char* ch;
    ch = strchr(bytes, '\n');
    str_copyb(&tmp, bytes, ch-bytes);
    bytes = ch + 1;
    ch = strchr(bytes, '\n');
    str_catb(&tmp, bytes, ch-bytes);
    msg1(tmp.s);
  }
  return 0;
}

static const response* message_end(void)
{
  str_copys(&tmp, "Received ");
  str_catu(&tmp, databytes);
  str_cats(&tmp, " bytes.");
  resp.message = tmp.s;
  return &resp;
}

struct plugin backend = {
  .reset = reset,
  .sender = sender,
  .recipient = recipient,
  .data_block = data_block,
  .message_end = message_end,
};
