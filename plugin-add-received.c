#include <bglibs/systime.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bglibs/msg.h>
#include "mailfront.h"

static str received;
static str fixup_host;
static str fixup_ip;

static const char* linkproto;
static const char* local_host;
static str local_ip;
static const char* remote_host;
static str remote_ip;

static const char* date_string(void)
{
  static char datebuf[64];
  time_t now = time(0);
  struct tm* tm = gmtime(&now);
  strftime(datebuf, sizeof datebuf - 1, "%d %b %Y %H:%M:%S -0000", tm);
  return datebuf;
}

static int str_catfromby(str* s, const char* helo_domain,
			 const char* host, const str* ip)
{
  if (ip->len == 0)
    ip = 0;
  if (helo_domain == 0)
    helo_domain = (host != 0) ? host : (ip != 0) ? ip->s : UNKNOWN;
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
	  !str_cat(s, ip) ||
	  !str_catc(s, ']'))
	return 0;
    if (!str_catc(s, ')')) return 0;
  }
  return 1;
}

static int fixup_received(str* s)
{
  if (local_host &&
      local_ip.len > 0 &&
      fixup_host.len > 0 &&
      fixup_ip.len > 0 &&
      (strcasecmp(local_host, fixup_host.s) != 0 ||
       strcasecmp(local_ip.s, fixup_ip.s) != 0)) {
    if (!str_cat3s(s, "Received: from ", local_host, " (")) return 0;
    if (!str_cat4s(s, local_host, " [", local_ip.s, "])\n"
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
  const char* add = session_getenv("HEADER_ADD");
  if (add != 0) {
    if (!str_cats(s, add)) return 0;
    if (!str_catc(s, '\n')) return 0;
  }
  return 1;
}

static int build_received(str* s)
{
  if (!str_cats(s, "Received: from ")) return 0;
  if (!str_catfromby(s, session_getstr("helo_domain"),
		     remote_host, &remote_ip))
    return 0;
  if (!str_cats(s, "\n  by ")) return 0;
  if (!str_catfromby(s, local_host, 0, &local_ip)) return 0;
  if (!str_cat4s(s, "\n  with ", session_protocol(),
		 " via ", linkproto))
    return 0;
  if (!str_cat3s(s, "; ", date_string(), "\n")) return 0;
  return 1;
}

static int str_copyip(str* s, const char* ip, int is_ipv6)
{
  s->len = 0;
  if (ip != 0) {
    if (is_ipv6
	&& !str_copys(s, "IPv6:"))
      return 0;
    return str_cats(s, ip);
  }
  return 1;
}

static const response* init(void)
{
  const char* tmp;
  int is_ipv6;

  linkproto = getprotoenv(0);
  is_ipv6 = linkproto != 0 && strcasecmp(linkproto, "TCP6") == 0;
  if (!str_copyip(&local_ip, getprotoenv("LOCALIP"), is_ipv6)) return &resp_oom;
  if (!str_copyip(&remote_ip, getprotoenv("REMOTEIP"), is_ipv6)) return &resp_oom;
  local_host = getprotoenv("LOCALHOST");
  remote_host = getprotoenv("REMOTEHOST");

  if ((tmp = getenv("FIXUP_RECEIVED_HOST")) != 0) {
    if (!str_copys(&fixup_host, tmp)) return &resp_oom;
    str_strip(&fixup_host);
  }
  if ((tmp = getenv("FIXUP_RECEIVED_IP")) != 0) {
    if (!str_copys(&fixup_ip, tmp)) return &resp_oom;
    str_strip(&fixup_ip);
  }

  return 0;
}

static const response* data_start(int fd)
{
  received.len = 0;
  if (!fixup_received(&received) ||
      !add_header_add(&received) ||
      !build_received(&received))
    return &resp_internal;
  return backend_data_block(received.s, received.len);
  (void)fd;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .init = init,
  .data_start = data_start,
};
