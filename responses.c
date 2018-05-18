#include "responses.h"

RESPONSE(accepted_message, 250, "2.6.0 Message accepted.");
RESPONSE(accepted_sender, 250, "2.1.0 Sender accepted.");
RESPONSE(accepted_recip, 250, "2.1.5 Recipient accepted.");
RESPONSE(internal, 451, "4.3.0 Internal error.");
RESPONSE(oom, 451, "4.3.0 Out of memory.");

int number_ok(const response* r)
{
  return (r->number & RESPONSE_MASK) < 400;
}

int response_ok(const response* r)
{
  return r == 0 || number_ok(r);
}
