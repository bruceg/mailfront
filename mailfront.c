#include <stdlib.h>
#include <string.h>

#include <systime.h>
#include <msg/msg.h>
#include <str/str.h>

#include "mailfront.h"

static RESPONSE(no_sender,550,"5.1.0 Mail system is not configured to accept that sender");
static RESPONSE(no_rcpt,550,"5.1.0 Mail system is not configured to accept that recipient");

const char UNKNOWN[] = "unknown";

const int msg_show_pid = 1;
const char program[] = "mailfront";

const int authenticating = 0;
extern void set_timeout(void);
extern void report_io_bytes(void);

#define MODULE_CALL(NAME,PARAMS,SHORT) do{ \
  struct plugin* plugin; \
  const response* tmp; \
  for (plugin = plugin_list; plugin != 0; plugin = plugin->next) { \
    if (plugin->NAME != 0) { \
      if ((tmp = plugin->NAME PARAMS) != 0) { \
        resp = tmp; \
        if (!response_ok(resp)) \
          return resp; \
        else if (SHORT) \
          break; \
      } \
    } \
  } \
} while(0)

const response* handle_init(void)
{
  const response* resp;

  atexit(report_io_bytes);

  set_timeout();

  MODULE_CALL(init, (), 0);

  if (session.backend->init != 0)
    return session.backend->init();
  return 0;
}

const response* handle_helo(str* host)
{
  const response* resp;
  MODULE_CALL(helo, (host), 0);
  session_setstr("helo_domain", host->s);
  return 0;
}

const response* handle_reset(void)
{
  const response* resp = 0;
  if (session.backend->reset != 0)
    session.backend->reset();
  MODULE_CALL(reset, (), 0);
  return resp;
}

const response* handle_sender(str* sender)
{
  const response* resp = 0;
  const response* tmpresp = 0;
  MODULE_CALL(sender, (sender), 1);
  if (resp == 0)
    return &resp_no_sender;
  if (session.backend->sender != 0)
    if (!response_ok(tmpresp = session.backend->sender(sender)))
      return tmpresp;
  if (resp == 0 || resp->message == 0) resp = tmpresp;
  return resp;
}

const response* handle_recipient(str* recip)
{
  const response* resp = 0;
  const response* hresp = 0;
  MODULE_CALL(recipient, (recip), 1);
  if (resp == 0)
    return &resp_no_rcpt;
  if (session.backend->recipient != 0)
    if (!response_ok(hresp = session.backend->recipient(recip)))
      return hresp;
  if (resp == 0 || resp->message == 0) resp = hresp;
  return resp;
}

static const response* data_response;

const response* handle_data_start(void)
{
  const response* resp = 0;
  if (session.backend->data_start != 0)
    resp = session.backend->data_start();
  if (response_ok(resp))
    MODULE_CALL(data_start, (), 0);
  data_response = response_ok(resp)
    ? 0
    : resp;
  return resp;
}

void handle_data_bytes(const char* bytes, unsigned len)
{
  const response* r;
  struct plugin* plugin;
  if (!response_ok(data_response))
    return;
  for (plugin = plugin_list; plugin != 0; plugin = plugin->next)
    if (plugin->data_block != 0)
      if ((r = plugin->data_block(bytes, len)) != 0
	   && !response_ok(r)) {
	data_response = r;
	return;
      }
  if (session.backend->data_block != 0)
    session.backend->data_block(bytes, len);
}

const response* handle_data_end(void)
{
  const response* resp = 0;
  if (!response_ok(data_response))
    return data_response;
  MODULE_CALL(data_end, (), 0);
  return (session.backend->data_end != 0)
    ? session.backend->data_end()
    : resp;
}

int respond_line(unsigned number, int final,
		 const char* msg, unsigned long len)
{
  static str line;
  if (number >= 400) {
    line.len = 0;
    str_catu(&line, number);
    str_catc(&line, ' ');
    str_catb(&line, msg, len);
    msg1(line.s);
  }
  if (!session.protocol->respond_line(number, final, msg, len)) return 0;
  if (final)
    if (!obuf_flush(&outbuf))
      return 0;
  return 1;
}

int respond(const response* resp)
{
  const char* msg;
  const char* nl;
  for (msg = resp->message; (nl = strchr(msg, '\n')) != 0; msg = nl + 1)
    respond_line(resp->number, 0, msg, nl-msg);
  return respond_line(resp->number, 1, msg, strlen(msg));
}

int main(int argc, char* argv[])
{
  const response* resp;

  if (argc < 3)
    die1(111, "Protocol or backend name are missing from the command line");

  session_init();
  if ((resp = load_modules(argv[1], argv[2], (const char**)(argv+3))) != 0
      || (resp = handle_init()) != 0) {
    if (session.protocol != 0) {
      respond(resp);
      return 1;
    }
    else
      die1(1, resp->message);
  }

  if (session.protocol->init != 0)
    if (session.protocol->init())
      return 1;

  return session.protocol->mainloop();
}
