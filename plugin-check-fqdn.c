#include "mailfront.h"

static RESPONSE(nodomain,554,"5.1.2 Address is missing a domain name");
static RESPONSE(nofqdn,554,"5.1.2 Address does not contain a fully qualified domain name");

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

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .sender = either,
  .recipient = either,
};
