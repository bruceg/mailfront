#include <stdlib.h>
#include "mailfront.h"

static RESPONSE(ok, 250, 0);

static const response* do_recipient(str* recipient)
{
  const char* relayclient = session_getenv("RELAYCLIENT");
  if (relayclient != 0) {
    str_cats(recipient, relayclient);
    return &resp_ok;
  }
  else if (session.authenticated)
    return &resp_ok;
  return 0;
}

STRUCT_PLUGIN(relayclient) = {
  .recipient = do_recipient,
};
