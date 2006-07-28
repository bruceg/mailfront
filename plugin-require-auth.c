#include "mailfront.h"

static RESPONSE(mustauth, 530, "5.7.1 You must authenticate first.");

static const response* sender(str* s)
{
  if (session.authenticated == 0
      && session.relayclient == 0)
    return &resp_mustauth;
  return 0;
  (void)s;
}

STRUCT_PLUGIN(require_auth) = {
  .sender = sender,
};
