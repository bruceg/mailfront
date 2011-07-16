#include <ctype.h>
#include "mailfront.h"

static RESPONSE(nodomain,554,"5.1.2 Address is missing a domain name");
static RESPONSE(nofqdn,554,"5.1.2 Address does not contain a fully qualified domain name");
static RESPONSE(notemptyrcpt,554,"5.1.2 Recipient address may not be empty");
static RESPONSE(notemptysender,554,"5.1.2 Empty sender address prohibited");
static RESPONSE(wrongdomain,554,"5.1.2 Sender contains a disallowed domain");

static const response* check_fqdn(str* s)
{
  int at;
  int dot;
  const char* name;

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
  return 0;
}

static const response* check_domains(const str* s, const char* domains)
{
  unsigned int at;
  unsigned int i;
  int atend;

  if ((at = str_findlast(s, '@')) == (unsigned)-1) {
    return &resp_notemptysender;
  }
  ++at;
  for (;;) {
    /* Skip any leading or doubled colons */
    while (*domains == ':')
      ++domains;
    /* Hit end of string without a match */
    if (*domains == 0)
      return &resp_wrongdomain;

    for (atend = 0, i = 0; !atend && at+i < s->len; ++i) {
      if (tolower(s->s[at+i]) != tolower(*domains))
	break;
      ++domains;
      atend = *domains == ':' || *domains == 0;
    }
    if (atend && at+i == s->len)
      return 0;

    /* Skip to next domain in list */
    while (*domains != ':' && *domains != 0)
      ++domains;
  }
}

static const response* sender(str* s, str* params)
{
  const response* r;
  const char* domains;

  if (s->len != 0) {
    if ((r = check_fqdn(s)) != 0)
      return r;
  }
  if ((domains = session_getenv("SENDER_DOMAINS")) != 0)
    return check_domains(s, domains);
  return 0;
  (void)params;
}

static const response* recipient(str* s, str* params)
{
  if (s->len == 0)
    return &resp_notemptyrcpt;
  return check_fqdn(s);
  (void)params;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .sender = sender,
  .recipient = recipient,
};
