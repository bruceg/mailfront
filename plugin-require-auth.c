#include "mailfront.h"

static RESPONSE(mustauth, 530, "5.7.1 You must authenticate first.");

static const response* sender(str* s, str* p)
{
  if (!session_getnum("authenticated", 0)
      && session_getenv("RELAYCLIENT") == 0)
    return &resp_mustauth;
  return 0;
  (void)s;
  (void)p;
}

struct plugin plugin = {
  .sender = sender,
};
