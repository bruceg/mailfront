#include <stdlib.h>

#include <systime.h>
#include <str/str.h>

#include "mailfront.h"
#include "mailrules.h"
#include "smtp.h"

static RESPONSE(badbounce, 550, "Bounce messages should have a single recipient.");
static RESPONSE(too_long, 552, "Sorry, that message exceeds the maximum message length.");
static RESPONSE(hops, 554, "This message is looping, too many hops.");
static RESPONSE(internal, 451, "Internal error.");

static int is_bounce = 0;
static int rcpt_count = 0;

static str received;

int authenticated = 0;
const char* relayclient = 0;

unsigned long maxdatabytes = 0;
unsigned maxhops = 0;

extern void set_timeout(void);
extern void report_io_bytes(void);

const response* handle_init(void)
{
  const char* tmp;

  atexit(report_io_bytes);

  set_timeout();

  relayclient = getenv("RELAYCLIENT");

  maxhops = 0;
  if ((tmp = getenv("MAXHOPS")) != 0) maxhops = strtoul(tmp, 0, 10);
  if (maxhops == 0) maxhops = 100;
  
  if ((tmp = getenv("DATABYTES")) != 0) maxdatabytes = strtoul(tmp, 0, 10);
  else maxdatabytes = 0;

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
  if (((resp = rules_validate_sender(sender)) == 0 &&
       (resp = backend_validate_sender(sender)) == 0) ||
      number_ok(resp)) {
    tmpresp = backend_handle_sender(sender);
    if (resp == 0 || (tmpresp != 0 && !number_ok(tmpresp)))
      resp = tmpresp;
  }
  is_bounce = sender->len == 0;
  return resp;
}

const response* handle_recipient(str* recip)
{
  const response* resp;
  const response* hresp;
  if (is_bounce && rcpt_count > 0) return &resp_badbounce;
  ++rcpt_count;
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
  if ((hresp = backend_handle_recipient(recip)) != 0) {
    if (!number_ok(hresp))
      return hresp;
    else 
      if (resp == 0) resp = hresp;
  }
  return 0;
}


const char UNKNOWN[] = "unknown";

int build_received(str* line, const str* helo_domain, const char* proto)
{
  const char* env;
  char datebuf[32];
  time_t now = time(0);
  struct tm* tm = gmtime(&now);
  strftime(datebuf, sizeof datebuf - 1, "%d %b %Y %H:%M:%S -0000", tm);

  if (!str_copys(line, "Received: from ")) return 0;
  if ((env = getenv("TCPREMOTEHOST")) == 0) env = UNKNOWN;
  if (!str_cats(line, env)) return 0;
  if (helo_domain && helo_domain->len && str_diffs(helo_domain, env)) {
    if (!str_cats(line, " (HELO ")) return 0;
    if (!str_cat(line, helo_domain)) return 0;
    if (!str_catc(line, ')')) return 0;
  }
  if ((env = getenv("TCPREMOTEIP")) == 0) env = UNKNOWN;
  if (!str_cats(line, " (")) return 0;
  if (!str_cats(line, env)) return 0;
  if (!str_cats(line, ")\n  by ")) return 0;
  if ((env = getenv("TCPLOCALHOST")) == 0) env = UNKNOWN;
  if (!str_cats(line, env)) return 0;
  if ((env = getenv("TCPLOCALIP")) == 0) env = UNKNOWN;
  if (!str_cats(line, " (")) return 0;
  if (!str_cats(line, env)) return 0;
  if (!str_cats(line, ") with ")) return 0;
  if (!str_cats(line, proto)) return 0;
  if (!str_cats(line, "; ")) return 0;
  if (!str_cats(line, datebuf)) return 0;
  if (!str_catc(line, '\n')) return 0;
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
  if (is_bounce && rcpt_count > 1) return &resp_badbounce;
  resp = backend_handle_data_start();
  if (response_ok(resp)) {
    if (!build_received(&received, helo_domain, protocol))
      return &resp_internal;
    backend_handle_data_bytes(received.s, received.len);
  }
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
  data_bytes += len;
  if (data_response) return;
  if (data_bytes > maxdatabytes) {
    data_response = &resp_too_long;
    return;
  }
  for (i = 0, p = bytes; i < len; ++i, ++p) {
    char ch = *p;
    if (ch == LF) {
      if (linepos == 0) in_header = 0;
      linepos = 0;
      in_rec = in_dt = in_header;
    }
    else {
      if (in_header) {
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
