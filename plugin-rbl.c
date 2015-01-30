#include <string.h>

#include <bglibs/msg.h>
#include <bglibs/wrap.h>
#include <bglibs/dns.h>
#include <bglibs/ipv4.h>
#include <bglibs/str.h>

#include "mailfront.h"

static enum msgstatus { na, good, bad } msgstatus;

static response resp;
static RESPONSE(dnserror, 451, "4.3.0 DNS error doing RBL lookup");
static int debug = 0;

static const response* make_response(int code, const char* status, const char* msg)
{
  static str resp_str;
  unsigned int i;
  if (!str_copyf(&resp_str, "s{: }s", status, msg))
    return &resp_oom;
  for (i = 0; i < resp_str.len; ++i)
    if (resp_str.s[i] < 32 || resp_str.s[i] > 126)
      resp_str.s[i] = '?';
  resp.number = code;
  resp.message = resp_str.s;
  return 0;
}

static const char* make_name(const ipv4addr* ip, const char* rbl)
{
  char iprbuf[16];
  static str name;
  wrap_str(str_copyb(&name, iprbuf, fmt_ipv4addr_reverse(iprbuf, ip)));
  wrap_str(str_catc(&name, '.'));
  wrap_str(str_cats(&name, rbl));
  return name.s;
}

static const response* test_rbl(const char* rbl, enum msgstatus status, const ipv4addr* ip)
{
  static struct dns_result txt;
  int i;
  const char* query = make_name(ip, rbl);

  if (dns_txt(&txt, query) < 0)
    return &resp_dnserror;
  if (txt.count == 0)
    return 0;
  if (debug) {
    msgf("{rbl: }s{:}", rbl);
    for (i = 0; i < txt.count; ++i)
      msg1(txt.rr.name[i]);
  }
  msgstatus = status;
  return make_response(451, "Blocked", txt.rr.name[0]); /* 451 temporary, 553 permanent */
}

static const response* test_rbls(const char* rbls, enum msgstatus status, const ipv4addr* ip)
{
  const char* comma;
  const response* r;
  static str rbl;
  while ((comma = strchr(rbls, ',')) != 0) {
    wrap_str(str_copyb(&rbl, rbls, comma-rbls));
    if ((r = test_rbl(rbl.s, status, ip)) != 0)
      return r;
    rbls = comma + 1;
  }
  return test_rbl(rbls, status, ip);
}

static const response* init(void)
{
  const char* blacklist;
  const response* r;
  const char* e;
  ipv4addr ip;
  struct plugin* backend = 0;

  debug = session_getenv("RBL_DEBUG") != 0;

  if ((blacklist = session_getenv("RBL_BLACKLISTS")) == 0 || *blacklist == 0) {
    if (debug)
      msg1("{rbl: No blacklists, skipping}");
    return 0;
  }

  if ((e = session_getenv("RBL_BACKEND")) != 0) {
    if ((r = load_backend(&backend, e)) != 0)
      return r;
  }

  /* Can only handle IPv4 sessions */
  if ((e = session_getenv("TCPREMOTEIP")) == 0) {
    if (debug)
      msg1("{rbl: $TCPREMOTEIP is unset, skipping RBL tests}");
    return 0;
  }
  if (!ipv4_scan(e, &ip)) {
    msgf("{rbl: Cannot parse IP '}s{'}", e);
    return 0;
  }

  if ((r = test_rbls(blacklist, bad, &ip)) != 0)
    return r;
  if (msgstatus == bad && backend != 0) {
    if (backend->init != 0)
      if ((r = backend->init()) != 0)
	return r;
    switch_backend(backend);
    msgstatus = good;
  }
  return 0;
}

static const response* sender(str* address, str* params)
{
  return (msgstatus == bad) ? &resp : 0;
  (void)address;
  (void)params;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = 0,
  .init = init,
  .sender = sender,
};
