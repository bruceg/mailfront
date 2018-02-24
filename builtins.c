#include <stdlib.h>
#include "mailfront-internal.h"

static RESPONSE(accept,250,0);
static RESPONSE(ok, 250, 0);
static RESPONSE(mustauth, 530, "5.7.1 You must authenticate first.");
static response resp;

static const response* acceptor(str* s, str* params)
{
  return &resp_accept;
  (void)s;
  (void)params;
}

static const response* reject(str* s, str* params)
{
  const char* sr;
  if ((sr = session_getenv("SMTPREJECT")) != 0
      || (sr = session_getenv("REJECT")) != 0) {
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
  (void)params;
}

static const response* relayclient_recip(str* recipient, str* params)
{
  const char* relayclient = session_getenv("RELAYCLIENT");
  if (relayclient != 0) {
    str_cats(recipient, relayclient);
    return &resp_ok;
  }
  else if (session_getnum("authenticated", 0))
    return &resp_ok;
  return 0;
  (void)params;
}

static const response* require_auth(str* s, str* params)
{
  if (!session_getnum("authenticated", 0)
      && session_getenv("RELAYCLIENT") == 0)
    return &resp_mustauth;
  return 0;
  (void)s;
  (void)params;
}

struct plugin builtin_plugins[] = {
  {
    .name = "accept",
    .sender = acceptor,
    .recipient = acceptor,
  },
  {
    .name = "accept-recipient",
    .recipient = acceptor,
  },
  {
    .name = "accept-sender",
    .sender = acceptor,
  },
  {
    .name = "force-file",
    .flags = FLAG_NEED_FILE,
  },
  {
    .name = "reject",
    .sender = reject,
  },
  {
    .name = "relayclient",
    .recipient = relayclient_recip,
  },
  {
    .name = "require-auth",
    .sender = require_auth,
  },
  { .name = 0 }
};
