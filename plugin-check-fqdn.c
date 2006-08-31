#include "mailfront.h"

static RESPONSE(nodomain,554,"5.1.2 Address is missing a domain name");
static RESPONSE(nofqdn,554,"5.1.2 Address does not contain a fully qualified domain name");

static const response* either(str* s, str* p)
{
  int at;
  int dot;
  if (s->len > 0) {
    if ((at = str_findlast(s, '@')) <= 0)
      return &resp_nodomain;
    if ((dot = str_findlast(s, '.')) < at)
      return &resp_nofqdn;
  }
  return 0;
  (void)p;
}

struct plugin plugin = {
  .sender = either,
  .recipient = either,
};
