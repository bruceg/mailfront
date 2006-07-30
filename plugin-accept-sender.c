#include "mailfront.h"

static RESPONSE(accept,250,0);

static const response* address(str* s)
{
  return &resp_accept;
  (void)s;
}

STRUCT_PLUGIN(accept) = {
  .sender = address,
};
