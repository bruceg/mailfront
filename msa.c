#include "mailfront.h"

static RESPONSE(nodomain,554,"Address is missing a domain name");
static RESPONSE(nofqdn,554,"Address does not contain a fully qualified domain name");

static const response* check_fqdn(str* s)
{
  int at;
  int dot;
  if ((at = str_findlast(s, '@')) <= 0)
    return &resp_nodomain;
  if ((dot = str_findlast(s, '.')) < at)
    return &resp_nofqdn;
  return 0;
}

const response* msa_validate_init(void)
{
  return 0;
}

const response* msa_validate_sender(str* sender)
{
  if (sender->len > 0)
    return check_fqdn(sender);
  return 0;
}

const response* msa_validate_recipient(str* recipient)
{
  return check_fqdn(recipient);
}
