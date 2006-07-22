#include <string.h>
#include <msg/msg.h>
#include "mailfront.h"

static response r = { 250, 0 };
static str tmp;

static unsigned long databytes = 0;

void backend_handle_reset(void)
{
  databytes = 0;
}

const response* backend_handle_sender(str* sender)
{
  str_copys(&tmp, "Sender='");
  str_cat(&tmp, sender);
  str_cats(&tmp, "'.");
  r.message = tmp.s;
  return &r;
}

const response* backend_handle_recipient(str* recipient)
{
  str_copys(&tmp, "Recipient='");
  str_cat(&tmp, recipient);
  str_cats(&tmp, "'.");
  r.message = tmp.s;
  return &r;
}

const response* backend_handle_data_start(void)
{
  return 0;
}

void backend_handle_data_bytes(const char* bytes, unsigned long len)
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
}

const response* backend_handle_data_end(void)
{
  str_copys(&tmp, "Received ");
  str_catu(&tmp, databytes);
  str_cats(&tmp, " bytes.");
  r.message = tmp.s;
  return &r;
}
