#include <string.h>
#include <stdlib.h>
#include "systime.h"
#include "mailfront.h"
#include "smtp.h"
#include "iobuf/iobuf.h"

static str cmd;
static str arg;
static str addr;
static str helo_domain;

static       response resp_helo = { 0, 250, 0 };
static       response resp_ehlo0 = { 0,          250, 0 };
static const response resp_ehlo1 = { &resp_ehlo0, 250, "8BITMIME" };
static const response resp_ehlo =  { &resp_ehlo1, 250, "PIPELINING" };
static RESPONSE(no_mail, 503, "You must send MAIL FROM: first");
static RESPONSE(vrfy, 252, "Send some mail, I'll try my best.");
static RESPONSE(help, 214, "Help not available.");
static RESPONSE(mail_ok, 250, "Sender accepted.");
static RESPONSE(rcpt_ok, 250, "Recipient accepted.");
static RESPONSE(no_rcpt, 503, "You must send RCPT TO: first");
static RESPONSE(data_ok, 354, "End your message with a period.");
static RESPONSE(data_end, 250, "Message accepted.");
static RESPONSE(too_long, 552, "Sorry, that message exceeds the maximum message length.");
static RESPONSE(ok, 250, "OK");
static RESPONSE(hops, 554, "This message is looping, too many hops.");
static RESPONSE(unimp, 500, "Not implemented.");

static int saw_mail = 0;
static int saw_rcpt = 0;

static int get_line_nostrip(void)
{
  if (!ibuf_getstr(&inbuf, &line, NL)) return 0;
  if (inbuf.count == 0) return 0;
  /* Strip a single CR immediately before the final NL */
  if (line.s[line.len-2] == CR) {
    line.s[line.len-2] = NL;
    str_truncate(&line, line.len-1);
  }
  return 1;
}
  
static int parse_addr_arg(void)
{
  unsigned start;
  unsigned len;

  str_truncate(&addr, 0);
  for (start = 0; start < arg.len &&
	 (arg.s[start] != LBRACE && arg.s[start] != COLON); ++start) ;
  if (arg.s[start] != LBRACE)
    for (++start; start < arg.len &&
	   (arg.s[start] == SPACE || arg.s[start] == TAB); ++start) ;
  if (arg.s[start] == '<') ++start;
  if (start >= arg.len) return 1;
  len = arg.len - start;
  if (arg.s[arg.len-1] == '>') --len;
  return str_copyb(&addr, arg.s+start, len);
}

static int build_received(void)
{
  const char* env;
  char datebuf[32];
  time_t now = time(0);
  struct tm* tm = gmtime(&now);
  strftime(datebuf, sizeof datebuf - 1, "%d %b %Y %H:%M:%S -0000", tm);

  if (!str_copys(&line, "Received: from ")) return 0;
  if ((env = getenv("TCPREMOTEHOST")) == 0) env = UNKNOWN;
  if (!str_cats(&line, env)) return 0;
  if (helo_domain.len && str_diffs(&helo_domain, env)) {
    if (!str_cats(&line, " (HELO ")) return 0;
    if (!str_cat(&line, &helo_domain)) return 0;
    if (!str_catc(&line, ')')) return 0;
  }
  if ((env = getenv("TCPREMOTEIP")) == 0) env = UNKNOWN;
  if (!str_cats(&line, " (")) return 0;
  if (!str_cats(&line, env)) return 0;
  if (!str_cats(&line, ")\n  by ")) return 0;
  if ((env = getenv("TCPLOCALHOST")) == 0) env = UNKNOWN;
  if (!str_cats(&line, env)) return 0;
  if ((env = getenv("TCPLOCALIP")) == 0) env = UNKNOWN;
  if (!str_cats(&line, " (")) return 0;
  if (!str_cats(&line, env)) return 0;
  if (!str_cats(&line, ") with SMTP; ")) return 0;
  if (!str_cats(&line, datebuf)) return 0;
  if (!str_catc(&line, NL)) return 0;
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
  resp_helo.message = domain_name.s;
  return respond_resp(&resp_helo, 1);
}

static int EHLO(void)
{
  str_copy(&helo_domain, &arg);
  resp_ehlo0.message = domain_name.s;
  return respond_resp(&resp_ehlo, 1);
}

static int MAIL(void)
{
  const response* resp;
  handle_reset();
  parse_addr_arg();
  if ((resp = handle_sender(&addr)) == 0) resp = &resp_mail_ok;
  if (resp->number >= 200 && resp->number < 300) saw_mail = 1;
  return respond_resp(resp, 1);
}

static int RCPT(void)
{
  const response* resp;
  if (!saw_mail) return respond_resp(&resp_no_mail, 1);
  parse_addr_arg();
  if ((resp = handle_recipient(&addr)) == 0) resp = &resp_rcpt_ok;
  if (resp->number >= 200 && resp->number < 300) ++saw_rcpt;
  return respond_resp(resp, 1);
}

static int RSET(void)
{
  saw_mail = 0;
  saw_rcpt = 0;
  handle_reset();
  return respond_resp(&resp_ok, 1);
}

static int DATA(void)
{
  const response* resp;
  unsigned long bytes;
  unsigned received;
  unsigned deliveredto;
  int inbody;
  
  if (!saw_mail) return respond_resp(&resp_no_mail, 1);
  if (!saw_rcpt) return respond_resp(&resp_no_rcpt, 1);
  if ((resp = handle_data_start()) != 0) return respond_resp(resp, 1);
  if (!respond_resp(&resp_data_ok, 1)) return 0;
  if (!build_received()) return 0;
  handle_data_bytes(line.s, line.len);

  bytes = received = deliveredto = inbody = 0;
  for (;;) {
    unsigned offset = 0;
    if (!get_line_nostrip()) {
      handle_reset();
      return 0;
    }
    bytes += line.len;
    if (line.s[0] == PERIOD) {
      if (line.len == 2) break;
      offset = 1;
    }
    handle_data_bytes(line.s+offset, line.len-offset);
    if (!inbody) {
      if (line.len == offset + 1)
	inbody = 1;
      else if (strncasecmp(line.s+offset, "Received:", 9) == 0)
	++received;
      else if (strncasecmp(line.s+offset, "Delivered-To:", 13) == 0)
	++deliveredto;
    }
    if (resp != 0) return respond_resp(resp, 1);
  }
  if (databytes && bytes > databytes) {
    handle_reset();
    return respond_resp(&resp_too_long, 1);
  }
  if (received > maxhops || deliveredto > maxhops) {
    handle_reset();
    return respond_resp(&resp_hops, 1);
  }
  if ((resp = handle_data_end()) == 0) resp = &resp_data_end;
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
