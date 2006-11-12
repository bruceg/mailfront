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
  else if (session_getnum("authenticated", 0))
    return &resp_ok;
  return 0;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .recipient = do_recipient,
};
