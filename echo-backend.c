#include "mailfront.h"

static response r = { 0, 250, 0 };
static str tmp;

void handle_reset(void)
{
}

const response* handle_sender(str* sender)
{
  str_copys(&tmp, "Sender='");
  str_cat(&tmp, sender);
  str_cats(&tmp, "'.");
  r.message = tmp.s;
  return &r;
}

const response* handle_recipient(str* recipient)
{
  str_copys(&tmp, "Recipient='");
  str_cat(&tmp, recipient);
  str_cats(&tmp, "'.");
  r.message = tmp.s;
  return &r;
}

const response* handle_data_start(void)
{
  return 0;
}

static unsigned long databytes = 0;

void handle_data_bytes(const char* bytes, unsigned long len)
{
  databytes += len;
}

const response* handle_data_end(void)
{
  str_copys(&tmp, "Received ");
  str_catu(&tmp, databytes);
  str_cats(&tmp, " bytes.");
  r.message = tmp.s;
  return &r;
}
