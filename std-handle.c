#include <stdlib.h>
#include <string.h>

#include <systime.h>
#include <str/str.h>

#include "mailfront.h"
#include "mailrules.h"
#include "smtp.h"

static RESPONSE(badbounce, 550, "Bounce messages should have a single recipient.");
static RESPONSE(too_long, 552, "Sorry, that message exceeds the maximum message length.");
static RESPONSE(hops, 554, "This message is looping, too many hops.");
static RESPONSE(internal, 451, "Internal error.");
static RESPONSE(manyrcpt, 550, "Too many recipients");

const char UNKNOWN[] = "unknown";

static int is_bounce = 0;
static unsigned rcpt_count = 0;

static str received;

int authenticated = 0;
const char* relayclient = 0;

unsigned long maxdatabytes = 0;
unsigned maxhops = 0;
unsigned maxrcpts = 0;

extern void set_timeout(void);
extern void report_io_bytes(void);

static const char* local_host;
static const char* local_ip;
static const char* remote_host;
static const char* remote_ip;
static str fixup_host;
static str fixup_ip;
static const char* linkproto;

const char* getprotoenv(const char* name)
{
  static str fullname;
  if (!str_copy2s(&fullname, linkproto, name)) return 0;
  return getenv(fullname.s);
}

const response* handle_init(void)
{
  const char* tmp;
  const response* resp;

  atexit(report_io_bytes);

  set_timeout();

  if ((linkproto = getenv("PROTO")) == 0) linkproto = "TCP";
  if ((local_host = getprotoenv("LOCALHOST")) == 0) local_host = UNKNOWN;
  if ((local_ip = getprotoenv("LOCALIP")) == 0) local_ip = UNKNOWN;
  if ((remote_host = getprotoenv("REMOTEHOST")) == 0) remote_host = UNKNOWN;
  if ((remote_ip = getprotoenv("REMOTEIP")) == 0) remote_ip = UNKNOWN;
  if ((tmp = getenv("FIXUP_RECEIVED_HOST")) != 0) {
    if (!str_copys(&fixup_host, tmp)) return &resp_oom;
    str_strip(&fixup_host);
  }
  if ((tmp = getenv("FIXUP_RECEIVED_IP")) != 0) {
    if (!str_copys(&fixup_ip, tmp)) return &resp_oom;
    str_strip(&fixup_ip);
  }
  /* The value of maxdatabytes gets reset in handle_data_start below.
   * This is here simply to provide a value for SMTP to report in its
   * EHLO response. */
  maxdatabytes = rules_getenvu("DATABYTES");

  if ((resp = backend_validate_init()) != 0) return resp;

  return rules_init();
}

const response* handle_reset(void)
{
  const response* resp;
  is_bounce = 0;
  rcpt_count = 0;
  if ((resp = rules_reset()) != 0) return resp;
  backend_handle_reset();
  return 0;
}

int number_ok(const response* resp)
{
  return resp->number < 400;
}

int response_ok(const response* resp)
{
  return resp == 0 || number_ok(resp);
}

const response* handle_sender(str* sender)
{
  const response* resp;
  const response* tmpresp;
  /* Logic:
   * if rules_validate_sender returns a response, use it
   * else if backend_validate_sender returns a response, use it
   */
  resp = rules_validate_sender(sender);
  maxrcpts = rules_getenvu("MAXRCPTS");
  relayclient = rules_getenv("RELAYCLIENT");
  if (resp == 0)
    resp = backend_validate_sender(sender);
  if (!response_ok(resp))
    return resp;
  if (!response_ok(tmpresp = backend_handle_sender(sender)))
    return tmpresp;
  if (resp == 0) resp = tmpresp;
  is_bounce = sender->len == 0;
  return resp;
}

const response* handle_recipient(str* recip)
{
  const response* resp;
  const response* hresp;
  if (maxrcpts > 0 && rcpt_count > maxrcpts) return &resp_manyrcpt;
  if (++rcpt_count > 1 && is_bounce) return &resp_badbounce;
  if ((resp = rules_validate_recipient(recip)) != 0) {
    if (!number_ok(resp))
      return resp;
  }
  else if (relayclient != 0)
    str_cats(recip, relayclient);
  else if (authenticated)
    resp = 0;
  else if ((resp = backend_validate_recipient(recip)) != 0) {
    if (!number_ok(resp))
      return resp;
  }
  if (!response_ok(hresp = backend_handle_recipient(recip)))
    return hresp;
  if (resp == 0) resp = hresp;
  return resp;
}

static const char* date_string(void)
{
  static char datebuf[64];
  time_t now = time(0);
  struct tm* tm = gmtime(&now);
  strftime(datebuf, sizeof datebuf - 1, "%d %b %Y %H:%M:%S -0000", tm);
  return datebuf;
}

int add_header_add(str* s)
{
  const char* add = rules_getenv("HEADER_ADD");
  if (add != 0) {
    if (!str_cats(s, add)) return 0;
    if (!str_catc(s, '\n')) return 0;
  }
  return 1;
}

int fixup_received(str* s)
{
  if (local_host &&
      local_ip &&
      fixup_host.len > 0 &&
      fixup_ip.len > 0 &&
      (strcasecmp(local_host, fixup_host.s) != 0 ||
       strcasecmp(local_ip, fixup_ip.s) != 0)) {
    if (!str_cat5s(s, "Received: from ", local_host, " ([", local_ip, "])\n"
		   "  by ")) return 0;
    if (!str_cat(s, &fixup_host)) return 0;
    if (!str_cats(s, " ([")) return 0;
    if (!str_cat(s, &fixup_ip)) return 0;
    if (!str_cat3s(s, "]); ", date_string(), "\n")) return 0;
  }
  return 1;
}

int build_received(str* s, const str* helo_domain, const char* proto)
{
  if (!str_cats(s, "Received: from ")) return 0;
  if (helo_domain && helo_domain->len) {
    if (!str_cat(s, helo_domain)) return 0;
    if (str_diffs(helo_domain, remote_host)) {
      if (!str_cat3s(s, " (", remote_host, " [")) return 0;
    }
    else
      if (!str_cats(s, " ([")) return 0;
  }
  else
    if (!str_cat2s(s, remote_host, " ([")) return 0;
  if (!str_cat6s(s, remote_ip, "])\n"
		 "  by ", local_host, " ([", local_ip, "])\n"
		 "  with ")) return 0;
  if (!str_cat6s(s, proto, " via ", linkproto,
		 "; ", date_string(), "\n")) return 0;
  return 1;
}

static unsigned long data_bytes; /* Total number of data bytes */
static unsigned count_rec;	/* Count of the Received: headers */
static unsigned count_dt;	/* Count of the Delivered-To: headers */
static int in_header;		/* True until a blank line is seen */
static unsigned linepos;     /* The number of bytes since the last LF */
static int in_rec;	      /* True if we might be seeing Received: */
static int in_dt;	  /* True if we might be seeing Delivered-To: */
static const response* data_response;

const response* handle_data_start(const str* helo_domain,
				      const char* protocol)
{
  const response* resp;
  maxdatabytes = rules_getenvu("DATABYTES");
  if (is_bounce && rcpt_count > 1) return &resp_badbounce;
  resp = backend_handle_data_start();
  if (response_ok(resp)) {
    received.len = 0;
    if (!fixup_received(&received) ||
	!add_header_add(&received) ||
	!build_received(&received, helo_domain, protocol))
      return &resp_internal;
    backend_handle_data_bytes(received.s, received.len);
  }

  if ((maxhops = rules_getenvu("MAXHOPS")) == 0)
    maxhops = 100;
  
  patterns_init();
  data_bytes = 0;
  count_rec = 0;
  count_dt = 0;
  in_header = 1;
  linepos = 0;
  in_rec = 1;
  in_dt = 1;
  data_response = 0;
  return resp;
}

void handle_data_bytes(const char* bytes, unsigned len)
{
  unsigned i;
  const char* p;
  const response* r;
  data_bytes += len;
  if (data_response) return;
  if (maxdatabytes && data_bytes > maxdatabytes) {
    data_response = &resp_too_long;
    return;
  }
  if ((r = patterns_check(bytes, len)) != 0) {
    data_response = r;
    return;
  }
  for (i = 0, p = bytes; in_header && i < len; ++i, ++p) {
    char ch = *p;
    if (ch == LF) {
      if (linepos == 0) in_header = 0;
      linepos = 0;
      in_rec = in_dt = in_header;
    }
    else {
      if (in_header && linepos < 13) {
	if (in_rec) {
	  if (ch != "received:"[linepos] &&
	      ch != "RECEIVED:"[linepos])
	    in_rec = 0;
	  else if (linepos >= 8) {
	    in_dt = in_rec = 0;
	    if (++count_rec > maxhops) {
	      data_response = &resp_hops;
	      return;
	    }
	  }
	}
	if (in_dt) {
	  if (ch != "delivered-to:"[linepos] &&
	      ch != "DELIVERED-TO:"[linepos])
	    in_dt = 0;
	  else if (linepos >= 12) {
	    in_dt = in_rec = 0;
	    if (++count_dt > maxhops) {
	      data_response = &resp_hops;
	      return;
	    }
	  }
	}
	++linepos;
      }
    }
  }
  backend_handle_data_bytes(bytes, len);
}

const response* handle_data_end(void)
{
  if (data_response)
    return data_response;
  return backend_handle_data_end();
}
