#include "mailfront.h"

static RESPONSE(nodomain,554,"5.1.2 Address is missing a domain name");
static RESPONSE(nofqdn,554,"5.1.2 Address does not contain a fully qualified domain name");
static RESPONSE(notempty,554,"5.1.2 Recipient address may not be empty");

static const response* either(str* s)
{
  int at;
  int dot;
  const char* name;
  if (s->len > 0) {
    if ((at = str_findlast(s, '@')) <= 0) {
      if ((name = session_getenv("DEFAULTHOST")) != 0) {
	at = s->len;
	if (!str_catc(s, '@')
	    || !str_cats(s, name))
	  return &resp_oom;
      }
      else
	return &resp_nodomain;
    }
    if ((dot = str_findlast(s, '.')) < at) {
      if ((name = session_getenv("DEFAULTDOMAIN")) != 0) {
	if (!str_catc(s, '.')
	    || !str_cats(s, name))
	  return &resp_oom;
      }
      else
	return &resp_nofqdn;
    }
  }
  return 0;
}

static const response* recipient(str* s)
{
  if (s->len == 0)
    return &resp_notempty;
  return either(s);
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .sender = either,
  .recipient = recipient,
};
