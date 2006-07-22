#include <stdlib.h>
#include "mailfront.h"
#include "mailrules.h"

static RESPONSE(ok, 250, 0);

static const response* reset(struct module* module,
			     struct session* session)
{
  session->relayclient = getenv("RELAYCLIENT");
  return 0;
  (void)module;
}

static const response* do_sender(struct module* module,
				 struct session* session,
				 str* sender)
{
  session->relayclient = rules_getenv("RELAYCLIENT");
  return 0;
  (void)module;
  (void)sender;
}

static const response* do_recipient(struct module* module,
				    struct session* session,
				    str* recipient)
{
  if (session->relayclient != 0) {
    str_cats(recipient, session->relayclient);
    return &resp_ok;
  }
  else if (session->authenticated)
    return &resp_ok;
  return 0;
  (void)module;
}

struct module relayclient = {
  .reset = reset,
  .sender = do_sender,
  .recipient = do_recipient,
};
