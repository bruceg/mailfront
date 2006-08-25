#include <string.h>
#include <stdlib.h>
#include <systime.h>

#include "mailfront.h"
#include "smtp.h"
#include <cvm/sasl.h>

#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/iter.h>

static RESPONSE(authfail, 421, "4.3.0 Failed to initialize AUTH");

static str line = {0,0,0};
static str domain_name = {0,0,0};

static struct sasl_auth saslauth = { .prefix = "334 " };

static unsigned long maxnotimpl = 0;

static str str_welcome;
static str cmd;
static str arg;
static str addr;
static str params;

static RESPONSE(no_mail, 503, "5.5.1 You must send MAIL FROM: first");
static RESPONSE(vrfy, 252, "2.5.2 Send some mail, I'll try my best.");
static RESPONSE(help, 214, "2.0.0 Help not available.");
static RESPONSE(mail_ok, 250, "2.1.0 Sender accepted.");
static RESPONSE(rcpt_ok, 250, "2.1.5 Recipient accepted.");
static RESPONSE(no_rcpt, 503, "5.5.1 You must send RCPT TO: first");
static RESPONSE(data_ok, 354, "End your message with a period on a line by itself.");
static RESPONSE(ok, 250, "2.3.0 OK");
static RESPONSE(unimp, 500, "5.5.1 Not implemented.");
static RESPONSE(needsparam, 501, "5.5.2 That command requires a parameter.");
static RESPONSE(auth_already, 503, "5.5.1 You are already authenticated.");
static RESPONSE(toobig, 552, "5.2.3 The message would exceed the maximum message size.");
static RESPONSE(toomanyunimp, 503, "5.5.0 Too many unimplemented commands.\n5.5.0 Closing connection.");
static RESPONSE(goodbye, 221, "2.0.0 Good bye.");
static RESPONSE(authenticated, 235, "2.7.0 Authentication succeeded.");
static response resp_tmp;

static int saw_mail = 0;
static int saw_rcpt = 0;

static int parse_addr_arg(void)
{
  unsigned i;
  char term;
  int quoted;
  
  if (!str_truncate(&addr, 0)) return 0;
  if (!str_truncate(&params, 0)) return 0;

  addr.len = 0;
  if ((i = str_findfirst(&arg, LBRACE) + 1) != 0)
    term = RBRACE;
  else {
    term = SPACE;
    if ((i = str_findfirst(&arg, COLON) + 1) == 0)
      if ((i = str_findfirst(&arg, SPACE) + 1) == 0)
	return 0;
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
      if (!str_catc(&addr, arg.s[i])) return 0;
    }
  }
  ++i;
  while (i < arg.len && arg.s[i] == SPACE) ++i;
  if (!str_copyb(&params, arg.s+i, arg.len-i)) return 0;
  str_subst(&params, ' ', 0);

  /* strip source routing */
  if (addr.s[0] == AT
      && (i = str_findfirst(&addr, COLON) + 1) != 0)
    str_lcut(&addr, i);
    
  return 1;
}

static const char* find_param(const char* name)
{
  const long len = strlen(name);
  striter i;
  for (striter_start(&i, &params, 0);
       striter_valid(&i);
       striter_advance(&i)) {
    if (strncasecmp(i.startptr, name, len) == 0) {
      if (i.startptr[len] == '0')
	return i.startptr + len;
      if (i.startptr[len] == '=')
	return i.startptr + len + 1;
    }
  }
  return 0;
}

static int QUIT(void)
{
  smtp_respond(&resp_goodbye);
  exit(0);
  return 0;
}

static int HELP(void)
{
  return smtp_respond(&resp_help);
}

static int HELO(void)
{
  const response* resp;
  if ((resp = handle_helo(&arg)) != 0)
    return smtp_respond(resp);
  resp_tmp.number = 250;
  resp_tmp.message = domain_name.s;
  return smtp_respond(&resp_tmp);
}

static int EHLO(void)
{
  static str auth_resp;
  session.protocol->name = "ESMTP";
  const response* resp;
  if ((resp = handle_helo(&arg)) != 0)
    return smtp_respond(resp);

  if (!str_copy(&line, &domain_name)) return 0;
  if (!str_catc(&line, '\n')) return 0;
  switch (sasl_auth_caps(&auth_resp)) {
  case 0: break;
  case 1:
    if (!str_cat(&line, &auth_resp)) return 0;
    if (!str_catc(&line, '\n')) return 0;
    break;
  default: return smtp_respond(&resp_internal);
  }
  if (!str_cats(&line, "SIZE ")) return 0;
  if (!str_catu(&line, session.maxdatabytes)) return 0;
  if (!str_cats(&line, "\n8BITMIME\nENHANCEDSTATUSCODES\nPIPELINING"))
    return 0;
  resp_tmp.number = 250;
  resp_tmp.message = line.s;
  return smtp_respond(&resp_tmp);
}

static void do_reset(void)
{
  const response* resp;
  if ((resp = handle_reset()) != 0) {
    smtp_respond(resp);
    exit(0);
  }
  saw_rcpt = 0;
  saw_mail = 0;
}

// FIXME: if rules_reset fails, exit
static int MAIL(void)
{
  const response* resp;
  const char* param;
  unsigned long size;
  msg2("MAIL ", arg.s);
  do_reset();
  parse_addr_arg();
  if ((resp = handle_sender(&addr)) == 0)
    resp = &resp_mail_ok;
  if (number_ok(resp)) {
    /* Look up the size limit after handling the sender,
       in order to honour limits set in the mail rules. */
    if (session.maxdatabytes > 0 &&
	(param = find_param("SIZE")) != 0 &&
	(size = strtoul(param, (char**)&param, 10)) > 0 &&
	*param == 0 &&
	size > session.maxdatabytes)
      return smtp_respond(&resp_toobig);
    saw_mail = 1;
  }
  return smtp_respond(resp);
}

static int RCPT(void)
{
  const response* resp;
  msg2("RCPT ", arg.s);
  if (!saw_mail) return smtp_respond(&resp_no_mail);
  parse_addr_arg();
  if ((resp = handle_recipient(&addr)) == 0)
    resp = &resp_rcpt_ok;
  if (number_ok(resp)) saw_rcpt = 1;
  return smtp_respond(resp);
}

static int RSET(void)
{
  do_reset();
  return smtp_respond(&resp_ok);
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
      if (sawperiod && linepos == 0) { data_end(); return 1; }
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
  
  if (!saw_mail) return smtp_respond(&resp_no_mail);
  if (!saw_rcpt) return smtp_respond(&resp_no_rcpt);
  if ((resp = handle_data_start()) != 0)
    return smtp_respond(resp);
  if (!smtp_respond(&resp_data_ok)) return 0;

  if (!copy_body()) {
    do_reset();
    return 0;
  }
  if ((resp = handle_data_end()) == 0)
    resp = &resp_accepted;
  return smtp_respond(resp);
}

static int NOOP(void)
{
  return smtp_respond(&resp_ok);
}

static int VRFY(void)
{
  return smtp_respond(&resp_vrfy);
}

static int AUTH(void)
{
  int i;
  if (session.authenticated) return smtp_respond(&resp_auth_already);
  if (arg.len == 0) return smtp_respond(&resp_needsparam);
  if ((i = sasl_auth1(&saslauth, &arg)) != 0) {
    const char* msg = sasl_auth_msg(&i);
    resp_tmp.number = i;
    resp_tmp.message = msg;
    return smtp_respond(&resp_tmp);
  }
  else {
    session.authenticated = 1;
    session.helo_domain = 0;
    smtp_respond(&resp_authenticated);
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
  static unsigned long notimpl = 0;
  struct dispatch* d;
  if (!parse_line()) return 1;
  for (d = dispatch_table; d->cmd != 0; ++d)
    if (strcasecmp(d->cmd, cmd.s) == 0) {
      notimpl = 0;
      return d->fn();
    }
  msg3(cmd.s, " ", arg.s);
  if (maxnotimpl > 0 && ++notimpl > maxnotimpl) {
    smtp_respond(&resp_toomanyunimp);
    return 0;
  }
  return smtp_respond(&resp_unimp);
}

static int init(void)
{
  const char* tmp;

  if ((tmp = getenv("TCPLOCALHOST")) == 0) tmp = UNKNOWN;
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

  return 0;
}

static int mainloop(void)
{
  if (!sasl_auth_init(&saslauth))
    return smtp_respond(&resp_authfail);

  resp_tmp.number = 220;
  resp_tmp.message = str_welcome.s;
  if (!smtp_respond(&resp_tmp)) return 1;
  while (ibuf_getstr_crlf(&inbuf, &line))
    if (!smtp_dispatch()) {
      if (ibuf_eof(&inbuf))
	msg1("Connection dropped");
      if (ibuf_timedout(&inbuf))
	msg1("Timed out");
      return 1;
    }
  return 0;
}

struct protocol protocol = {
  .name = "SMTP",
  .respond = smtp_respond,
  .init = init,
  .mainloop = mainloop,
};
