#include <stdlib.h>
#include "mailfront.h"

static RESPONSE(ok, 250, 0);

static const response* reset(void)
{
  session.relayclient = getenv("RELAYCLIENT");
  return 0;
}

static const response* do_sender(str* sender)
{
  session.relayclient = session_getenv("RELAYCLIENT");
  return 0;
  (void)sender;
}

static const response* do_recipient(str* recipient)
{
  if (session.relayclient != 0) {
    str_cats(recipient, session.relayclient);
    return &resp_ok;
  }
  else if (session.authenticated)
    return &resp_ok;
  return 0;
}

struct module relayclient = {
  .reset = reset,
  .sender = do_sender,
  .recipient = do_recipient,
};
