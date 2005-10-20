#include <stdlib.h>
#include <msg/wrap.h>
#include "mailfront.h"

static RESPONSE(nodomain,554,"Address is missing a domain name");
static RESPONSE(nofqdn,554,"Address does not contain a fully qualified domain name");

const char *defaultdomain;
const char *defaulthost;

static const response* check_fqdn(str* s)
{
  int at;
  int dot;
  if ((at = str_findlast(s, '@')) <= 0) {
    if (defaulthost == 0)
      return &resp_nodomain;
    else {
      at = s->len;
      wrap_str(str_catc(s, '@'));
      wrap_str(str_cats(s, defaulthost));
    }
  }
  if ((dot = str_findlast(s, '.')) < at) {
    if (defaultdomain == 0)
      return &resp_nofqdn;
    else {
      wrap_str(str_catc(s, '.'));
      wrap_str(str_cats(s, defaultdomain));
    }
  }
  return 0;
}

const response* msa_validate_init(void)
{
  defaultdomain = getenv("DEFAULTDOMAIN");
  defaulthost = getenv("DEFAULTHOST");
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
