#include <stdlib.h>
#include "mailfront.h"
#include "qmtp.h"
#include <iobuf/iobuf.h>
#include "qmail.h"

const char program[] = "smtpfront-qmail";

void backend_handle_reset(void)
{
  qmail_reset();
}

const response* backend_validate_sender(str* sender)
{
  return qmail_validate_sender(sender);
}

const response* backend_handle_sender(str* sender)
{
  return qmail_sender(sender);
}

const response* backend_validate_recipient(str* recip)
{
  if (relayclient != 0) {
    str_cats(recip, relayclient);
    return 0;
  }
  else if (authenticated)
    return 0;
  else
    return qmail_validate_recipient(recip);
}

const response* backend_handle_recipient(str* recip)
{
  return qmail_recipient(recip);
}

const response* backend_handle_data_start(void)
{
  return qmail_data_start();
}

void backend_handle_data_bytes(const char* bytes, unsigned long len)
{
  qmail_data_bytes(bytes, len);
}

const response* backend_handle_data_end(void)
{
  return qmail_data_end();
}

int main(void)
{
  const response* resp;
  if ((resp = qmail_validate_init()) != 0) { respond_resp(resp, 1); return 1; }
  return qmtp_mainloop();
}
