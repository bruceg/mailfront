#include <stdlib.h>
#include <string.h>
#include <bglibs/systime.h>

#include "mailfront.h"
#include "starttls.h"

#include <bglibs/iobuf.h>
#include <bglibs/msg.h>

extern struct protocol protocol;

static str line = {0,0,0};
static str domain_name = {0,0,0};

static unsigned long maxnotimpl = 0;

static str str_welcome;
static str cmd;
static str arg;
static str addr;
static str params;
static str init_capabilities;

static RESPONSE(no_mail, 503, "5.5.1 You must send MAIL FROM: first");
static RESPONSE(vrfy, 252, "2.5.2 Send some mail, I'll try my best.");
static RESPONSE(help, 214, "2.0.0 Help not available.");
static RESPONSE(no_rcpt, 503, "5.5.1 You must send RCPT TO: first");
static RESPONSE(data_ok, 354, "End your message with a period on a line by itself.");
static RESPONSE(ok, 250, "2.3.0 OK");
static RESPONSE(unimp, 500, "5.5.1 Not implemented.");
static RESPONSE(badaddr, 501, "5.5.2 Syntax error in address parameter.");
static RESPONSE(needsparam, 501, "5.5.2 Syntax error, command requires a parameter.");
static RESPONSE(noparam, 501, "5.5.2 Syntax error, no parameters allowed.");
static RESPONSE(toomanyunimp, 503, "5.5.0 Too many unimplemented commands.\n5.5.0 Closing connection.");
static RESPONSE(goodbye, 221, "2.0.0 Good bye.");
static RESPONSE(starttls, 220, "2.0.0 Ready to start TLS");

static int saw_mail = 0;
static int saw_rcpt = 0;

static const response* parse_addr_arg(void)
{
  unsigned i;
  char term;
  int quoted;
  
  if (!str_truncate(&addr, 0)) return &resp_oom;
  if (!str_truncate(&params, 0)) return &resp_oom;

  addr.len = 0;
  if ((i = str_findfirst(&arg, LBRACE) + 1) != 0)
    term = RBRACE;
  else {
    term = SPACE;
    if ((i = str_findfirst(&arg, COLON) + 1) == 0)
      if ((i = str_findfirst(&arg, SPACE) + 1) == 0)
	return &resp_badaddr;
    while (i < arg.len && arg.s[i] == SPACE)
      ++i;
  }

  for (quoted = 0; i < arg.len && (quoted || arg.s[i] != term); ++i) {
    switch (arg.s[i]) {
    case QUOTE:
      quoted = !quoted;
      break;
    case ESCAPE:
      ++i;
      /* fall through */
    default:
      if (!str_catc(&addr, arg.s[i])) return &resp_oom;
    }
  }
  ++i;
  if (i > arg.len)
    return (addr.len > 0 && term == SPACE) ? 0 : &resp_badaddr;
  while (i < arg.len && arg.s[i] == SPACE) ++i;
  if (!str_copyb(&params, arg.s+i, arg.len-i)) return &resp_oom;
  str_subst(&params, ' ', 0);

  /* strip source routing */
  if (addr.s[0] == AT
      && (i = str_findfirst(&addr, COLON) + 1) != 0)
    str_lcut(&addr, i);
    
  return 0;
}

static int QUIT(void)
{
  respond(&resp_goodbye);
  exit(0);
  return 0;
}

static int HELP(void)
{
  return respond(&resp_help);
}

static int HELO(void)
{
  const response* resp;
  if (response_ok(resp = handle_reset()))
    resp = handle_helo(&arg, &line);
  return (resp != 0) ? respond(resp)
    : respond_line(250, 1, domain_name.s, domain_name.len);
}

static int EHLO(void)
{
  const response* resp;
  protocol.name = "ESMTP";
  line.len = 0;
  if (!response_ok(resp = handle_reset())
      || !response_ok(resp = handle_helo(&arg, &line)))
    return respond(resp);
  if (!str_cat(&line, &init_capabilities)) {
    respond(&resp_oom);
    return 0;
  }

  if (!respond_line(250, 0, domain_name.s, domain_name.len)) return 0;
  return respond_multiline(250, 1, line.s);
}

static void do_reset(void)
{
  const response* resp;
  if (!response_ok(resp = handle_reset())) {
    respond(resp);
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
  if ((resp = parse_addr_arg()) == 0) {
    if ((resp = handle_sender(&addr, &params)) == 0)
      resp = &resp_accepted_sender;
    if (number_ok(resp)) {
      saw_mail = 1;
    }
  }
  return respond(resp);
}

static int RCPT(void)
{
  const response* resp;
  msg2("RCPT ", arg.s);
  if (!saw_mail) return respond(&resp_no_mail);
  if ((resp = parse_addr_arg()) == 0) {
    if ((resp = handle_recipient(&addr, &params)) == 0)
      resp = &resp_accepted_recip;
    if (number_ok(resp))
      saw_rcpt = 1;
  }
  return respond(resp);
}

static int RSET(void)
{
  do_reset();
  return respond(&resp_ok);
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
static void message_end(void)
{
  if (data_bufpos)
    handle_data_bytes(data_buf, data_bufpos);
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
      if (sawperiod && linepos == 0) { message_end(); return 1; }
      data_byte(ch);
      sawperiod = sawcr = linepos = 0;
      break;
    case CR:
      if (sawcr) { data_byte(CR); ++linepos; }
      sawcr = 1;
      break;
    default:
      if (ch == PERIOD && !sawperiod && linepos == 0)
	sawperiod = 1;
      else {
	sawperiod = 0;
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
  
  if (!saw_mail) return respond(&resp_no_mail);
  if (!saw_rcpt) return respond(&resp_no_rcpt);
  if (!response_ok(resp = handle_data_start()))
    return respond(resp);
  if (!respond(&resp_data_ok)) return 0;

  if (!copy_body()) {
    do_reset();
    return 0;
  }
  if ((resp = handle_message_end()) == 0)
    resp = &resp_accepted_message;
  return respond(resp);
}

static int NOOP(void)
{
  return respond(&resp_ok);
}

static int VRFY(void)
{
  return respond(&resp_vrfy);
}

static int STARTTLS(void)
{
  if (!starttls_available())
    return respond(&resp_unimp);
  if (!respond(&resp_starttls))
    return 0;
  if (!starttls_start())
    return 0;
  starttls_disable();
  session_setnum("tls_state", 1);
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
  { "STARTTLS", STARTTLS },
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

int smtp_dispatch(const struct command* commands)
{
  static unsigned long notimpl = 0;
  struct dispatch* d;
  const struct command* c;
  if (!parse_line()) return 1;
  for (c = commands; c->name != 0; c++)
    if (strcasecmp(c->name, cmd.s) == 0) {
      if (c->fn_enabled == 0 || c->fn_enabled()) {
	notimpl = 0;
	if (arg.len == 0) {
	  if (c->fn_noparam == 0)
	    return respond(&resp_noparam);
	  return c->fn_noparam();
	}
	else {
	  if (c->fn_hasparam == 0)
	    return respond(&resp_needsparam);
	  return c->fn_hasparam(&arg);
	}
      }
    }
  for (d = dispatch_table; d->cmd != 0; ++d)
    if (strcasecmp(d->cmd, cmd.s) == 0) {
      notimpl = 0;
      return d->fn();
    }
  msg3(cmd.s, " ", arg.s);
  if (maxnotimpl > 0 && ++notimpl > maxnotimpl) {
    respond(&resp_toomanyunimp);
    return 0;
  }
  return respond(&resp_unimp);
}

static int init(void)
{
  const char* tmp;
  const response* resp;

  if ((tmp = getprotoenv("LOCALHOST")) == 0) tmp = UNKNOWN;
  str_copys(&domain_name, tmp);

  if ((tmp = getenv("SMTPGREETING")) != 0)
    str_copys(&str_welcome, tmp);
  else {
    str_copy(&str_welcome, &domain_name);
    str_cats(&str_welcome, " mailfront");
  }
  str_cats(&str_welcome, " ESMTP");

  if ((tmp = getenv("MAXNOTIMPL")) != 0)
    maxnotimpl = strtoul(tmp, 0, 10);

  if (!str_cats(&init_capabilities, "8BITMIME\nENHANCEDSTATUSCODES\nPIPELINING")) {
    respond(&resp_oom);
    return 1;
  }

  if ((resp = starttls_init()) != NULL) {
    respond(resp);
    return 1;
  }

  if (starttls_available()) {
    if (!str_cats(&init_capabilities, "\nSTARTTLS")) {
      respond(&resp_oom);
      return 1;
    }
  }

  return 0;
}

static int mainloop(const struct command* commands)
{
  if (!respond_line(220, 1, str_welcome.s, str_welcome.len)) return 0;
  while (ibuf_getstr_crlf(&inbuf, &line))
    if (!smtp_dispatch(commands)) {
      if (ibuf_eof(&inbuf))
	msg1("Connection dropped");
      if (ibuf_timedout(&inbuf))
	msg1("Timed out");
      return 1;
    }
  return 0;
}

static int smtp_respond_line(unsigned num, int final,
			     const char* msg, unsigned long len)
{
  return obuf_putu(&outbuf, num)
    && obuf_putc(&outbuf, final ? ' ' : '-')
    && obuf_write(&outbuf, msg, len)
    && obuf_puts(&outbuf, CRLF);
}

struct protocol protocol = {
  .version = PROTOCOL_VERSION,
  .name = "SMTP",
  .respond_line = smtp_respond_line,
  .init = init,
  .mainloop = mainloop,
};
