#include <stdlib.h>
#include <string.h>

#include <systime.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"
#include "smtp.h"

const char UNKNOWN[] = "unknown";

const int msg_show_pid = 1;
const int authenticating = 0;
extern void set_timeout(void);
extern void report_io_bytes(void);

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
  struct plugin* plugin; \
  for (plugin = plugin_list; plugin != 0; plugin = plugin->next) { \
    if (plugin->NAME != 0) { \
      if ((resp = plugin->NAME PARAMS) != 0) { \
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

  if ((session.linkproto = getenv("PROTO")) == 0)
    session.linkproto = "TCP";
  getprotoenv("LOCALIP", &session.local_ip);
  getprotoenv("REMOTEIP", &session.remote_ip);
  getprotoenv("LOCALHOST", &session.local_host);
  getprotoenv("REMOTEHOST", &session.remote_host);

  if ((resp = load_plugins()) != 0)
    return resp;
  
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
  struct plugin* plugin;
  if (data_response) return;
  for (plugin = plugin_list; plugin != 0; plugin = plugin->next)
    if (plugin->data_block != 0)
      if ((r = plugin->data_block(bytes, len)) != 0
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
  MODULE_CALL(data_end, ());
  return backend_handle_data_end();
}

int main(void)
{
  return protocol_mainloop();
}
