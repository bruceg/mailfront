#include "mailfront.h"

static RESPONSE(accept,250,0);

static const response* address(str* s)
{
  return &resp_accept;
  (void)s;
}

struct plugin plugin = {
  .sender = address,
};
