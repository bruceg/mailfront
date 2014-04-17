#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <systime.h>
#include <msg/msg.h>
#include <path/path.h>
#include <str/str.h>

#include "mailfront-internal.h"

static RESPONSE(no_sender,550,"5.1.0 Mail system is not configured to accept that sender");
static RESPONSE(no_rcpt,550,"5.1.0 Mail system is not configured to accept that recipient");

const char UNKNOWN[] = "unknown";

const int msg_show_pid = 1;
const char program[] = "mailfront";

const int authenticating = 0;
extern void set_timeout(void);
extern void report_io_bytes(void);

static str tmp_prefix;

static str no_params;

static void exitfn(void)
{
  handle_reset();
  report_io_bytes();
}

#define MODULE_CALL(NAME,PARAMS,RESET) do{ \
  struct plugin* plugin; \
  const response* tmp; \
  for (plugin = session.plugin_list; plugin != 0; plugin = plugin->next) { \
    if (plugin->NAME != 0) { \
      if ((tmp = plugin->NAME PARAMS) != 0) { \
        if (!response_ok(tmp)) {	      \
          if (RESET)			      \
            handle_reset();		      \
          return tmp;			      \
        }				      \
        if (resp == 0)			      \
	  resp = tmp;			      \
      } \
    } \
  } \
} while(0)

static const response* handle_init(void)
{
  const response* resp = 0;

  atexit(exitfn);

  set_timeout();

  MODULE_CALL(init, (), 0);

  if (session.backend->init != 0)
    return session.backend->init();
  return 0;
}

const response* handle_helo(str* host, str* capabilities)
{
  const response* resp = 0;
  MODULE_CALL(helo, (host, capabilities), 0);
  session_setstr("helo_domain", host->s);
  if (session.backend->helo != 0)
    return session.backend->helo(host, capabilities);
  return 0;
}

const response* handle_reset(void)
{
  const response* resp = 0;
  if (session.fd >= 0) {
    close(session.fd);
    session.fd = -1;
  }
  if (session.backend->reset != 0)
    session.backend->reset();
  MODULE_CALL(reset, (), 0);
  return resp;
}

const response* handle_sender(str* sender, str* params)
{
  const response* resp = 0;
  const response* tmpresp = 0;
  if (params == 0) {
    no_params.len = 0;
    params = &no_params;
  }
  MODULE_CALL(sender, (sender, params), 0);
  if (resp == 0)
    return &resp_no_sender;
  if (session.backend->sender != 0)
    if (!response_ok(tmpresp = session.backend->sender(sender, params)))
      return tmpresp;
  if (resp == 0 || resp->message == 0) resp = tmpresp;
  return resp;
}

const response* handle_recipient(str* recip, str* params)
{
  const response* resp = 0;
  const response* hresp = 0;
  if (params == 0) {
    no_params.len = 0;
    params = &no_params;
  }
  MODULE_CALL(recipient, (recip, params), 0);
  if (resp == 0)
    return &resp_no_rcpt;
  if (session.backend->recipient != 0)
    if (!response_ok(hresp = session.backend->recipient(recip, params)))
      return hresp;
  if (resp == 0 || resp->message == 0) resp = hresp;
  return resp;
}

static const response* data_response;

const response* handle_data_start(void)
{
  const response* resp = 0;
  if (session.fd >= 0) {
    close(session.fd);
    session.fd = -1;
  }
  if (session.flags & FLAG_NEED_FILE) {
    if ((session.fd = scratchfile()) == -1)
      return &resp_internal;
  }
  data_response = 0;
  if (session.backend->data_start != 0)
    resp = session.backend->data_start(session.fd);
  if (response_ok(resp))
    MODULE_CALL(data_start, (session.fd), 1);
  if (!response_ok(resp)) {
    handle_reset();
    data_response = resp;
  }
  return resp;
}

void handle_data_bytes(const char* bytes, unsigned len)
{
  const response* r;
  struct plugin* plugin;
  if (!response_ok(data_response))
    return;
  for (plugin = session.plugin_list; plugin != 0; plugin = plugin->next)
    if (plugin->data_block != 0)
      if ((r = plugin->data_block(bytes, len)) != 0
	   && !response_ok(r)) {
	handle_reset();
	data_response = r;
	return;
      }
  if (session.backend->data_block != 0)
    session.backend->data_block(bytes, len);
  if (session.fd >= 0)
    if (write(session.fd, bytes, len) != (ssize_t)len) {
      handle_reset();
      data_response = &resp_internal;
    }
}

const response* handle_message_end(void)
{
  const response* resp = 0;
  if (!response_ok(data_response))
    return data_response;
  MODULE_CALL(message_end, (session.fd), 1);
  if (session.backend->message_end != 0)
    resp = session.backend->message_end(session.fd);
  if (session.fd >= 0)
    close(session.fd);
  session.fd = -1;
  if (!response_ok(resp))
    handle_reset();
  return resp;
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

int respond_multiline(unsigned number, int final, const char* msg)
{
  const char* nl;
  while ((nl = strchr(msg, '\n')) != 0) {
    if (!respond_line(number, 0, msg, nl-msg))
      return 0;
    msg = nl + 1;
  }
  return respond_line(number, final, msg, strlen(msg));
}

int respond(const response* resp)
{
  return respond_multiline(resp->number, 1, resp->message);
}

const response* backend_data_block(const char* data, unsigned long len)
{
  return (session.fd >= 0)
    ? (
       (write(session.fd, data, len) != (ssize_t)len)
       ? &resp_internal
       : 0
       )
    : (
       (session.backend->data_block != 0)
       ? session.backend->data_block(data, len)
       : 0
       );
}

int scratchfile(void)
{
  str filename = {0,0,0};
  int fd;
  if ((fd = path_mktemp(tmp_prefix.s, &filename)) != -1)
    unlink(filename.s);
  str_free(&filename);
  return fd;
}

struct command* commands;

static int collect_commands(void)
{
  const struct plugin* plugin;
  const struct command* cmd;
  int i = 0;

  for (cmd = session.backend->commands; cmd != 0 && cmd->name != 0; ++cmd, ++i) ;
  for (plugin = session.plugin_list; plugin != 0; plugin = plugin->next)
    for (cmd = plugin->commands; cmd != 0 && cmd->name != 0; ++cmd, ++i) ;

  if ((commands = malloc((i+1) * sizeof *commands)) == 0)
    return 0;

  i = 0;
  for (cmd = session.backend->commands; cmd != 0 && cmd->name != 0; ++cmd, ++i)
    commands[i] = *cmd;
  for (plugin = session.plugin_list; plugin != 0; plugin = plugin->next)
    for (cmd = plugin->commands; cmd != 0 && cmd->name != 0; ++cmd, ++i)
      commands[i] = *cmd;
  commands[i].name = 0;

  return 1;
}

int main(int argc, char* argv[])
{
  const response* resp;
  const char* tmp;

  if (argc < 3)
    die1(111, "Protocol or backend name are missing from the command line");

  if ((tmp = getenv("TMPDIR")) == 0)
    tmp = "/tmp";
  if (!str_copy2s(&tmp_prefix, tmp, "/mailfront.tmp.")) die_oom(111);
  
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

  if (!collect_commands()) {
    respond(&resp_oom);
    return 1;
  }

  if (session.protocol->init != 0)
    if (session.protocol->init())
      return 1;

  return session.protocol->mainloop(commands);
}
