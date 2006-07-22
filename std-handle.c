#include <stdlib.h>
#include <string.h>

#include <systime.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"
#include "mailrules.h"
#include "smtp.h"

static RESPONSE(too_long, 552, "5.2.3 Sorry, that message exceeds the maximum message length.");
static RESPONSE(hops, 554, "5.6.0 This message is looping, too many hops.");
static RESPONSE(manyrcpt, 550, "5.5.3 Too many recipients");
static RESPONSE(mustauth, 530, "5.7.1 You must authenticate first.");
static RESPONSE(nodomain,554,"5.1.2 Address is missing a domain name");
static RESPONSE(nofqdn,554,"5.1.2 Address does not contain a fully qualified domain name");

const char UNKNOWN[] = "unknown";

static unsigned rcpt_count = 0;

const int authenticating = 0;
extern void set_timeout(void);
extern void report_io_bytes(void);
extern struct module backend_validate;
extern struct module cvm_validate;
extern struct module relayclient;
extern struct module add_received;

static struct module* module_list = 0;
static struct module* module_tail = 0;

static const char* require_auth;

void add_module(struct module* module)
{
  if (module_tail == 0)
    module_list = module;
  else
    module_tail->next = module;
  module_tail = module;
}

static const response* check_fqdn(const str* s)
{
  int at;
  int dot;
  if ((at = str_findlast(s, '@')) <= 0)
    return &resp_nodomain;
  if ((dot = str_findlast(s, '.')) < at)
    return &resp_nofqdn;
  return 0;
}

static void getprotoenv(const char* name, const char** dest)
{
  static str fullname;
  const char* env;
  if (!str_copy2s(&fullname, session.linkproto, name)) die_oom(111);
  if ((env = getenv(fullname.s)) != 0
      && env[0] == 0)
    env = 0;
  *dest = env;
}

#define MODULE_CALL(NAME,PARAMS) do{ \
  struct module* module; \
  for (module = module_list; module != 0; module = module->next) { \
    if (module->NAME != 0) { \
      if ((resp = module->NAME PARAMS) != 0) { \
        if (response_ok(resp)) \
          break; \
        else \
          return resp; \
      } \
    } \
  } \
} while(0)

const response* handle_init(void)
{
  const response* resp;

  atexit(report_io_bytes);

  set_timeout();

  require_auth = getenv("REQUIRE_AUTH");
  if ((session.linkproto = getenv("PROTO")) == 0)
    session.linkproto = "TCP";
  getprotoenv("LOCALIP", &session.local_ip);
  getprotoenv("REMOTEIP", &session.remote_ip);
  getprotoenv("LOCALHOST", &session.local_host);
  getprotoenv("REMOTEHOST", &session.remote_host);
  /* The value of maxdatabytes gets reset in handle_data_start below.
   * This is here simply to provide a value for SMTP to report in its
   * EHLO response. */
  session.maxdatabytes = rules_getenvu("DATABYTES");

  add_module(&relayclient);
  add_module(&backend_validate);
  add_module(&cvm_validate);
  add_module(&add_received);

  MODULE_CALL(init, ());

  return rules_init();
}

const response* handle_reset(void)
{
  const response* resp;
  rcpt_count = 0;
  if ((resp = rules_reset()) != 0) return resp;
  backend_handle_reset();
  MODULE_CALL(reset, ());
  return resp;
}

const response* handle_sender(str* sender)
{
  const response* resp;
  const response* tmpresp;
  if (require_auth && !(session.authenticated || session.relayclient != 0))
    return &resp_mustauth;
  /* Logic:
   * if rules_validate_sender returns a response, use it
   * else if backend_validate_sender returns a response, use it
   */
  resp = rules_validate_sender(sender);
  session.maxrcpts = rules_getenvu("MAXRCPTS");
  session.maxdatabytes = rules_getenvu("DATABYTES");
  if (resp == 0 && sender->len > 0)
    resp = check_fqdn(sender);
  if (!response_ok(resp))
    return resp;
  if (resp == 0)
    MODULE_CALL(sender, (sender));
  if (!response_ok(tmpresp = backend_handle_sender(sender)))
    return tmpresp;
  if (resp == 0 || resp->message == 0) resp = tmpresp;
  return resp;
}

const response* handle_recipient(str* recip)
{
  const response* resp;
  const response* hresp;
  ++rcpt_count;
  if (session.maxrcpts > 0 && rcpt_count > session.maxrcpts)
    return &resp_manyrcpt;
  if ((resp = rules_validate_recipient(recip)) != 0) {
    if (!number_ok(resp))
      return resp;
  }
  else if ((resp = check_fqdn(recip)) != 0)
    return resp;
  else
    MODULE_CALL(recipient, (recip));
  if (!response_ok(hresp = backend_handle_recipient(recip)))
    return hresp;
  if (resp == 0 || resp->message == 0) resp = hresp;
  return resp;
}

static unsigned long data_bytes; /* Total number of data bytes */
static unsigned count_rec;	/* Count of the Received: headers */
static unsigned count_dt;	/* Count of the Delivered-To: headers */
static int in_header;		/* True until a blank line is seen */
static unsigned linepos;     /* The number of bytes since the last LF */
static int in_rec;	      /* True if we might be seeing Received: */
static int in_dt;	  /* True if we might be seeing Delivered-To: */
static const response* data_response;

const response* handle_data_start(void)
{
  const response* resp;
  session.maxdatabytes = rules_getenvu("DATABYTES");
  resp = backend_handle_data_start();
  if (resp == 0)
    MODULE_CALL(data_start, ());
  if (response_ok(resp)) {
    if ((session.maxhops = rules_getenvu("MAXHOPS")) == 0)
      session.maxhops = 100;
  
    patterns_init();
    data_bytes = 0;
    count_rec = 0;
    count_dt = 0;
    in_header = 1;
    linepos = 0;
    in_rec = 1;
    in_dt = 1;
    data_response = 0;
  }
  return resp;
}

void handle_data_bytes(const char* bytes, unsigned len)
{
  unsigned i;
  const char* p;
  const response* r;
  struct module* module;
  data_bytes += len;
  if (data_response) return;
  for (module = module_list; module != 0; module = module->next)
    if (module->data_block != 0)
      if ((r = module->data_block(bytes, len)) != 0
	   && !response_ok(r)) {
	data_response = r;
	return;
      }
  if (session.maxdatabytes && data_bytes > session.maxdatabytes) {
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
	    if (++count_rec > session.maxhops) {
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
	    if (++count_dt > session.maxhops) {
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
  const response* resp;
  if (data_response)
    return data_response;
  MODULE_CALL(data_end, (module, session));
  return backend_handle_data_end();
}
