#include <stdlib.h>
#include <string.h>

#include <systime.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"
#include "smtp.h"

const char UNKNOWN[] = "unknown";

const int msg_show_pid = 1;
const char program[] = "mailfront";

const int authenticating = 0;
extern void set_timeout(void);
extern void report_io_bytes(void);
extern struct plugin* backend;
extern struct protocol* protocol;

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

  MODULE_CALL(init, ());

  return 0;
}

const response* handle_reset(void)
{
  const response* resp = 0;
  if (backend->reset != 0)
    backend->reset();
  MODULE_CALL(reset, ());
  return resp;
}

const response* handle_sender(str* sender)
{
  const response* resp = 0;
  const response* tmpresp = 0;
  MODULE_CALL(sender, (sender));
  if (backend->sender != 0)
    if (!response_ok(tmpresp = backend->sender(sender)))
      return tmpresp;
  if (resp == 0 || resp->message == 0) resp = tmpresp;
  return resp;
}

const response* handle_recipient(str* recip)
{
  const response* resp = 0;
  const response* hresp = 0;
  MODULE_CALL(recipient, (recip));
  if (backend->recipient != 0)
    if (!response_ok(hresp = backend->recipient(recip)))
      return hresp;
  if (resp == 0 || resp->message == 0) resp = hresp;
  return resp;
}

static const response* data_response;

const response* handle_data_start(void)
{
  const response* resp = 0;
  if (backend->data_start != 0)
    resp = backend->data_start();
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
  if (backend->data_block != 0)
    backend->data_block(bytes, len);
}

const response* handle_data_end(void)
{
  const response* resp;
  if (data_response)
    return data_response;
  MODULE_CALL(data_end, ());
  return (backend->data_end != 0)
    ? backend->data_end()
    : 0;
}

int main(int argc, char* argv[])
{
  const response* resp;
  const char* backend_name;
  const char* p;
  int i;
  str argv0 = {0,0,0};
  str protocol_name = {0,0,0};

  str_copys(&argv0, argv[0]);
  if ((i = str_findlast(&argv0, '/')) > 0)
    str_lcut(&argv0, i + 1);
  
  if (argc > 2)
    backend_name = argv[2];
  else if (argc > 1)
    backend_name = argv[1];
  else if ((backend_name = strchr(argv0.s, '-')) != 0)
    ++backend_name;
  else
    die1(111, "Could not determine backend name");

  if (argc > 2)
    str_copys(&protocol_name, argv[1]);
  else if ((p = strstr(argv0.s, "front")) != 0)
    str_copyb(&protocol_name, argv0.s, p - argv0.s);
  else
    die1(111, "Could not determine protocol name");
  
  if ((resp = load_modules(protocol_name.s, backend_name)) != 0
      || (resp = handle_init()) != 0) {
    if (protocol != 0) {
      protocol->respond(resp);
      return 1;
    }
    else
      die1(1, resp->message);
  }
  str_free(&argv0);
  str_free(&protocol_name);

  session.protocol = protocol->name;
  if (protocol->init != 0)
    if (protocol->init())
      return 1;

  return protocol->mainloop();
}
