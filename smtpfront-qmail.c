#include <stdlib.h>
#include "mailfront.h"
#include "smtp.h"
#include "iobuf/iobuf.h"
#include "qmail.h"

const char program[] = "smtpd-qmail";

static const char* relayclient;

void handle_reset(void)
{
  qmail_reset();
}

const response* handle_sender(str* sender)
{
  const response* resp;
  if ((resp = qmail_validate_sender(sender)) != 0) return resp;
  return qmail_sender(sender);
}

const response* handle_recipient(str* recip)
{
  const response* resp;
  if (relayclient)
    str_cats(recip, relayclient);
  else
    if ((resp = qmail_validate_recipient(recip)) != 0) return resp;
  return qmail_recipient(recip);
}

const response* handle_data_start(void)
{
  return qmail_data_start();
}

void handle_data_bytes(const char* bytes, unsigned long len)
{
  qmail_data_bytes(bytes, len);
}

const response* handle_data_end(void)
{
  return qmail_data_end();
}

int main(void)
{
  const response* resp;

  relayclient = getenv("RELAYCLIENT");
  
  if ((resp = qmail_validate_init()) != 0) { respond_resp(resp, 1); return 1; }
  return smtp_mainloop(0);
}
