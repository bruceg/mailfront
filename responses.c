#include "responses.h"

RESPONSE(accepted, 250, "2.6.0 Message accepted.");
RESPONSE(internal, 451, "4.3.0 Internal error.");
RESPONSE(oom, 451, "4.3.0 Out of memory.");

int number_ok(const response* r)
{
  return r->number < 400;
}

int response_ok(const response* r)
{
  return r == 0 || number_ok(r);
}
