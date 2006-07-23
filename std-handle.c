#include <stdlib.h>
#include <string.h>

#include <systime.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"
#include "smtp.h"

static RESPONSE(mustauth, 530, "5.7.1 You must authenticate first.");

const char UNKNOWN[] = "unknown";

const int authenticating = 0;
extern void set_timeout(void);
extern void report_io_bytes(void);
extern struct module backend_validate;
extern struct module cvm_validate;
extern struct module relayclient;
extern struct module add_received;
extern struct module check_fqdn;
extern struct module patterns;
extern struct module counters;
extern struct module mailrules;

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

  add_module(&check_fqdn);
  add_module(&counters);
  add_module(&mailrules);
  add_module(&relayclient);
  add_module(&backend_validate);
  add_module(&cvm_validate);
  add_module(&add_received);
  add_module(&patterns);

  MODULE_CALL(init, ());

  return 0;
}

const response* handle_reset(void)
{
  const response* resp = 0;
  backend_handle_reset();
  MODULE_CALL(reset, ());
  return resp;
}

const response* handle_sender(str* sender)
{
  const response* resp = 0;
  const response* tmpresp;
  if (require_auth && !(session.authenticated || session.relayclient != 0))
    return &resp_mustauth;
  MODULE_CALL(sender, (sender));
  if (!response_ok(tmpresp = backend_handle_sender(sender)))
    return tmpresp;
  if (resp == 0 || resp->message == 0) resp = tmpresp;
  return resp;
}

const response* handle_recipient(str* recip)
{
  const response* resp = 0;
  const response* hresp;
  MODULE_CALL(recipient, (recip));
  if (!response_ok(hresp = backend_handle_recipient(recip)))
    return hresp;
  if (resp == 0 || resp->message == 0) resp = hresp;
  return resp;
}

static const response* data_response;

const response* handle_data_start(void)
{
  const response* resp;
  resp = backend_handle_data_start();
  if (resp == 0)
    MODULE_CALL(data_start, ());
  if (response_ok(resp)) {
    data_response = 0;
  }
  return resp;
}

void handle_data_bytes(const char* bytes, unsigned len)
{
  const response* r;
  struct module* module;
  if (data_response) return;
  for (module = module_list; module != 0; module = module->next)
    if (module->data_block != 0)
      if ((r = module->data_block(bytes, len)) != 0
	   && !response_ok(r)) {
	data_response = r;
	return;
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
