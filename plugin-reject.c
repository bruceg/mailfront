#include <stdlib.h>
#include "mailfront.h"

static response resp;

static const response* sender(str* s)
{
  const char* sr;
  if ((sr = getenv("SMTPREJECT")) != 0
      || (sr = getenv("REJECT")) != 0) {
    if (sr[0] == '-') {
      ++sr;
      resp.number = 553;
    }
    else
      resp.number = 451;
    resp.message = (sr[0] != 0)
      ? sr
      : "You are not allowed to use this mail server.";
    return &resp;
  }
  return 0;
  (void)s;
}

struct plugin plugin = {
  .sender = sender,
};
