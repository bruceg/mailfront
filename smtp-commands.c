#include <string.h>
#include <stdlib.h>
#include <systime.h>

#include "mailfront.h"
#include "mailrules.h"
#include "smtp.h"
#include "sasl-auth.h"

#include <iobuf/iobuf.h>
#include <msg/msg.h>

static str cmd;
static str arg;
static str addr;
static str params;
static str helo_domain;

static const response resp_ehlo0 = { 0,           250, "8BITMIME" };
static const response resp_ehlo  = { &resp_ehlo0, 250, "PIPELINING" };
static RESPONSE(no_mail, 503, "You must send MAIL FROM: first");
static RESPONSE(vrfy, 252, "Send some mail, I'll try my best.");
static RESPONSE(help, 214, "Help not available.");
static RESPONSE(mail_ok, 250, "Sender accepted.");
static RESPONSE(rcpt_ok, 250, "Recipient accepted.");
static RESPONSE(no_rcpt, 503, "You must send RCPT TO: first");
static RESPONSE(data_ok, 354, "End your message with a period.");
static RESPONSE(data_end, 250, "Message accepted.");
static RESPONSE(ok, 250, "OK");
static RESPONSE(unimp, 500, "Not implemented.");
static RESPONSE(internal, 451, "Internal error.");
static RESPONSE(needsparam, 501, "That command requires a parameter.");
static RESPONSE(auth_already, 503, "You are already authenticated.");

static int saw_mail = 0;
static int saw_rcpt = 0;
static const char* smtp_mode = "SMTP";

static int parse_addr_arg(void)
{
  unsigned start;
  unsigned len;
  unsigned end;
  
  if (!str_truncate(&addr, 0)) return 0;
  if (!str_truncate(&params, 0)) return 0;
  
  for (start = 0; start < arg.len &&
	 (arg.s[start] != LBRACE && arg.s[start] != COLON); ++start) ;
  if (arg.s[start] != LBRACE)
    for (++start; start < arg.len &&
	   (arg.s[start] == SPACE || arg.s[start] == TAB); ++start) ;
  if (arg.s[start] == LBRACE) ++start;
  if (start >= arg.len) return 1;
  if ((end = str_findnext(&arg, RBRACE, start)) == (unsigned)-1) end = arg.len;
  len = end - start;
  if (!str_copyb(&addr, arg.s+start, len)) return 0;
  if (arg.s[end] == RBRACE) ++end;
  while (arg.s[end] == SPACE) ++end;
  if (!str_copyb(&params, arg.s+end, arg.len-end)) return 0;
  return 1;
}

static int QUIT(void)
{
  respond(221, 1, "Good bye.");
  exit(0);
  return 0;
}

static int HELP(void)
{
  return respond_resp(&resp_help, 1);
}

static int HELO(void)
{
  str_copy(&helo_domain, &arg);
  return respond(250, 1, domain_name.s);
}

static int EHLO(void)
{
  static str auth_resp;
  smtp_mode = "ESMTP";
  str_copy(&helo_domain, &arg);
  if (!respond(250, 0, domain_name.s)) return 0;

  switch (sasl_auth_cap(&auth_resp)) {
  case 0: break;
  case 1: if (!respond(250, 0, auth_resp.s)) return 0; break;
  default: return respond_resp(&resp_internal, 1);
  }

  return respond_resp(&resp_ehlo, 1);
}

static void do_reset(void)
{
  const response* resp;
  if ((resp = std_handle_reset()) != 0) {
    respond_resp(resp, 1);
    exit(0);
  }
  saw_rcpt = 0;
  saw_mail = 0;
}

// FIXME: if rules_reset fails, exit
static int MAIL(void)
{
  const response* resp;
  msg2("MAIL ", arg.s);
  do_reset();
  parse_addr_arg();
  if ((resp = std_handle_sender(&addr)) == 0) resp = &resp_mail_ok;
  if (number_ok(resp)) saw_mail = 1;
  return respond_resp(resp, 1);
}

static int RCPT(void)
{
  const response* resp;
  msg2("RCPT ", arg.s);
  if (!saw_mail) return respond_resp(&resp_no_mail, 1);
  parse_addr_arg();
  if ((resp = std_handle_recipient(&addr)) == 0) resp = &resp_rcpt_ok;
  if (number_ok(resp)) saw_rcpt = 1;
  return respond_resp(resp, 1);
}

static int RSET(void)
{
  do_reset();
  return respond_resp(&resp_ok, 1);
}

static char data_buf[4096];
static unsigned long data_bytes;
static unsigned data_bufpos;
static void data_start(void)
{
  data_bytes = data_bufpos = 0;
}
static void data_byte(char ch)
{
  data_buf[data_bufpos++] = ch;
  ++data_bytes;
  if (data_bufpos >= sizeof data_buf) {
    std_handle_data_bytes(data_buf, data_bufpos);
    data_bufpos = 0;
  }
}
static void data_end(void)
{
  if (data_bufpos) std_handle_data_bytes(data_buf, data_bufpos);
}

static int copy_body(void)
{
  int sawcr = 0;		/* Was the last character a CR */
  unsigned linepos = 0;		/* The number of bytes since the last LF */
  int sawperiod = 0;		/* True if the first character was a period */
  char ch;

  data_start();
  while (ibuf_getc(&inbuf, &ch)) {
    switch (ch) {
    case LF:
      if (sawperiod && linepos == 0) { data_end(); return 1; }
      data_byte(ch);
      sawcr = linepos = 0;
      break;
    case CR:
      if (sawcr) { data_byte(CR); ++linepos; }
      sawcr = 1;
      break;
    default:
      if (ch == PERIOD && !sawperiod && linepos == 0)
	sawperiod = 1;
      else {
	if (sawcr) { data_byte(CR); ++linepos; sawcr = 0; }
	data_byte(ch); ++linepos;
      }
    }
  }
  return 0;
}

static int DATA(void)
{
  const response* resp;
  
  if (!saw_mail) return respond_resp(&resp_no_mail, 1);
  if (!saw_rcpt) return respond_resp(&resp_no_rcpt, 1);
  if ((resp = std_handle_data_start(&helo_domain, smtp_mode)) != 0)
    return respond_resp(resp, 1);
  if (!respond_resp(&resp_data_ok, 1)) return 0;

  if (!copy_body()) {
    do_reset();
    return 0;
  }
  if ((resp = std_handle_data_end()) == 0) resp = &resp_data_end;
  do_reset();
  return respond_resp(resp, 1);
}

static int NOOP(void)
{
  return respond_resp(&resp_ok, 1);
}

static int VRFY(void)
{
  return respond_resp(&resp_vrfy, 1);
}

static int AUTH(void)
{
  int i;
  if (authenticated) return respond_resp(&resp_auth_already, 1);
  if (arg.len == 0) return respond_resp(&resp_needsparam, 1);
  if ((i = sasl_auth1("334 ", &arg)) != 0) {
    const char* msg = sasl_auth_msg(&i);
    return respond(i, 1, msg);
  }
  else {
    authenticated = 1;
    str_truncate(&helo_domain, 0);
    respond(235, 1, "Authentication succeeded.");
  }
  return 1;
}

typedef int (*dispatch_fn)(void);
struct dispatch 
{
  const char* cmd;
  dispatch_fn fn;
};
static struct dispatch dispatch_table[] = {
  { "DATA", DATA },
  { "EHLO", EHLO },
  { "HELO", HELO },
  { "HELP", HELP },
  { "MAIL", MAIL },
  { "NOOP", NOOP },
  { "QUIT", QUIT },
  { "RCPT", RCPT },
  { "RSET", RSET },
  { "VRFY", VRFY },
  { "AUTH", AUTH },
  { 0, 0 }
};

static int parse_line(void)
{
  unsigned i;
  for (i = 0; i < line.len; i++) {
    if (line.s[i] == SPACE || line.s[i] == TAB) {
      unsigned j = i;
      while (j < line.len &&
	     (line.s[j] == SPACE || line.s[j] == TAB))
	++j;
      return str_copyb(&cmd, line.s, i) &&
	str_copyb(&arg, line.s+j, line.len-j);
    }
  }
  return str_copy(&cmd, &line) && str_truncate(&arg, 0);
}

int smtp_dispatch(void)
{
  struct dispatch* d;
  if (!parse_line()) return 1;
  for (d = dispatch_table; d->cmd != 0; ++d)
    if (strcasecmp(d->cmd, cmd.s) == 0)
      return d->fn();
  return respond_resp(&resp_unimp, 1);
}
