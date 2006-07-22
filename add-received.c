#include <systime.h>
#include <stdlib.h>
#include <string.h>
#include "mailfront.h"
#include "mailrules.h"

static str received;
static str fixup_host;
static str fixup_ip;

static const char* date_string(void)
{
  static char datebuf[64];
  time_t now = time(0);
  struct tm* tm = gmtime(&now);
  strftime(datebuf, sizeof datebuf - 1, "%d %b %Y %H:%M:%S -0000", tm);
  return datebuf;
}

static int str_catfromby(str* s, const char* helo_domain,
			 const char* host, const char* ip)
{
  if (helo_domain == 0)
    helo_domain = (host != 0) ? host : (ip != 0) ? ip : UNKNOWN;
  if (!str_cats(s, helo_domain)) return 0;
  if (host != 0 || ip != 0) {
    if (!str_cats(s, " (")) return 0;
    if (host != 0) {
      if (!str_cats(s, host)) return 0;
      if (ip != 0)
	if (!str_catc(s, ' ')) return 0;
    }
    if (ip != 0)
      if (!str_catc(s, '[') ||
	  !str_cats(s, ip) ||
	  !str_catc(s, ']'))
	return 0;
    if (!str_catc(s, ')')) return 0;
  }
  return 1;
}

static int fixup_received(struct session* session, str* s)
{
  if (session->local_host &&
      session->local_ip &&
      fixup_host.len > 0 &&
      fixup_ip.len > 0 &&
      (strcasecmp(session->local_host, fixup_host.s) != 0 ||
       strcasecmp(session->local_ip, fixup_ip.s) != 0)) {
    if (!str_cat3s(s, "Received: from ", session->local_host, " (")) return 0;
    if (!str_cat4s(s, session->local_host, " [", session->local_ip, "])\n"
		   "  by ")) return 0;
    if (!str_cat(s, &fixup_host)) return 0;
    if (!str_cats(s, " ([")) return 0;
    if (!str_cat(s, &fixup_ip)) return 0;
    if (!str_cat3s(s, "]); ", date_string(), "\n")) return 0;
  }
  return 1;
}

static int add_header_add(str* s)
{
  const char* add = rules_getenv("HEADER_ADD");
  if (add != 0) {
    if (!str_cats(s, add)) return 0;
    if (!str_catc(s, '\n')) return 0;
  }
  return 1;
}

static int build_received(struct session* session, str* s)
{
  if (!str_cats(s, "Received: from ")) return 0;
  if (!str_catfromby(s, session->helo_domain,
		     session->remote_host, session->remote_ip))
    return 0;
  if (!str_cats(s, "\n  by ")) return 0;
  if (!str_catfromby(s, session->local_host, 0, session->local_ip)) return 0;
  if (!str_cat4s(s, "\n  with ", session->protocol,
		 " via ", session->linkproto))
    return 0;
  if (!str_cat3s(s, "; ", date_string(), "\n")) return 0;
  return 1;
}

static const response* init(struct module* module,
			    struct session* session)
{
  const char* tmp;
  if ((tmp = getenv("FIXUP_RECEIVED_HOST")) != 0) {
    if (!str_copys(&fixup_host, tmp)) return &resp_oom;
    str_strip(&fixup_host);
  }
  if ((tmp = getenv("FIXUP_RECEIVED_IP")) != 0) {
    if (!str_copys(&fixup_ip, tmp)) return &resp_oom;
    str_strip(&fixup_ip);
  }
  return 0;
  (void)module;
  (void)session;
}

static const response* data_start(struct module* module,
				  struct session* session)
{
  received.len = 0;
  if (!fixup_received(session, &received) ||
      !add_header_add(&received) ||
      !build_received(session, &received))
    return &resp_internal;
  backend_handle_data_bytes(received.s, received.len);
  return 0;
  (void)module;
}

struct module add_received = {
  .init = init,
  .data_start = data_start,
};
