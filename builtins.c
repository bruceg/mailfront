#include "mailfront.h"

static RESPONSE(accept,250,0);

static const response* accept(str* s, str* p)
{
  return &resp_accept;
  (void)s;
  (void)p;
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
