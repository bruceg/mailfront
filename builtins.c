#include "mailfront-internal.h"

static RESPONSE(accept,250,0);

static const response* accept(str* s)
{
  return &resp_accept;
  (void)s;
}

struct plugin builtin_plugins[] = {
  {
    .name = "accept",
    .sender = accept,
    .recipient = accept,
  },
  {
    .name = "accept-recipient",
    .recipient = accept,
  },
  {
    .name = "accept-sender",
    .sender = accept,
  },
  { .name = 0 }
};
