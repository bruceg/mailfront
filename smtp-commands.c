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
    handle_data_bytes(data_buf, data_bufpos);
    data_bufpos = 0;
  }
}
static void data_end(void)
{
  if (data_bufpos) handle_data_bytes(data_buf, data_bufpos);
}

static int copy_body(unsigned* count_rec, unsigned* count_dt)
{
  int in_header = 1;		/* True until a blank line is seen */
  int sawcr = 0;		/* Was the last character a CR */
  unsigned linepos = 0;		/* The number of bytes since the last LF */
  int in_rec = 1;		/* True if we might be seeing Received: */
  int in_dt = 1;		/* True if we might be seeing Delivered-To: */
  int sawperiod = 0;		/* True if the first character was a period */
  char ch;

  *count_rec = *count_dt = 0;
  data_start();
  while (ibuf_getc(&inbuf, &ch)) {
    switch (ch) {
    case NL:
      if (sawperiod && linepos == 0) { data_end(); return 1; }
      data_byte(ch);
      if (linepos == 0) in_header = 0;
      sawcr = sawperiod = linepos = 0;
      in_rec = in_dt = in_header;
      break;
    case CR:
      if (sawcr) { data_byte(CR); ++linepos; }
      sawcr = 1;
      break;
    default:
      if (ch == PERIOD && !sawperiod && linepos == 0)
	sawperiod = 1;
      else {
	if (in_header) {
	  if (in_rec) {
	    if (ch != "received:"[linepos] &&
		ch != "RECEIVED:"[linepos])
	      in_rec = 0;
	    else if (linepos >= 8)
	      ++*count_rec, in_rec = 0;
	  }
	  if (in_dt) {
	    if (ch != "delivered-to:"[linepos] &&
		ch != "DELIVERED-TO:"[linepos])
	      in_dt = 0;
	    else if (linepos >= 12)
	      ++*count_dt, in_dt = 0;
	  }
	}
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
  unsigned received;
  unsigned deliveredto;
  
  if (!saw_mail) return respond_resp(&resp_no_mail, 1);
  if (!saw_rcpt) return respond_resp(&resp_no_rcpt, 1);
  if ((resp = handle_data_start()) != 0) return respond_resp(resp, 1);
  if (!respond_resp(&resp_data_ok, 1)) return 0;
  if (!build_received()) return 0;
  handle_data_bytes(line.s, line.len);

  if (!copy_body(&received, &deliveredto)) {
    handle_reset();
    return 0;
  }
  if (maxdatabytes && data_bytes > maxdatabytes) {
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
