/* imapfront-auth.c - IMAP authentication front-end
 * Copyright (C) 2008  Bruce Guenter <bruce@untroubled.org> or FutureQuest, Inc.
 * Development of this program was sponsored by FutureQuest, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Contact information:
 * FutureQuest Inc.
 * PO BOX 623127
 * Oviedo FL 32762-3127 USA
 * http://www.FutureQuest.net/
 * ossi@FutureQuest.net
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysdeps.h>
#include <cvm/sasl.h>
#include <cvm/v2client.h>
#include <iobuf/iobuf.h>
#include <str/str.h>

const char program[] = "imapfront-auth";
const int msg_show_pid = 1;

#define MAX_ARGC 16
#define QUOTE '"'
#define ESCAPE '\\'
#define LBRACE '{'
#define RBRACE '}'

static const char NOTAG[] = "*";
static const char CONT[] = "+";

static const char* capability;
static const char* cvm;
static const char* domain;
static char** nextcmd;
static str tag;
static str line;
static str cmd;
static str line_args[MAX_ARGC];
static int line_argc;

static struct sasl_auth saslauth = { .prefix = "+ " };

void log_start(const char* tagstr)
{
  obuf_puts(&errbuf, program);
  obuf_putc(&errbuf, '[');
  obuf_putu(&errbuf, getpid());
  obuf_puts(&errbuf, "]: ");
  if (tagstr) {
    obuf_puts(&errbuf, tagstr);
    obuf_putc(&errbuf, ' ');
  }
}

void log_str(const char* msg) { obuf_puts(&errbuf, msg); }
void log_end(void) { obuf_putsflush(&errbuf, "\n"); }

void log(const char* tagstr, const char* msg)
{
  log_start(tagstr);
  log_str(msg);
  log_end();
}

void respond_start(const char* tagstr)
{
  if (tagstr == 0) tagstr = tag.s;
  log_start(tagstr);
  if (!obuf_puts(&outbuf, tagstr) ||
      !obuf_putc(&outbuf, ' '))
    exit(1);
}

void respond_str(const char* msg)
{
  log_str(msg);
  if (!obuf_puts(&outbuf, msg))
    exit(1);
}

void respond_end(void)
{
  log_end();
  if (!obuf_putsflush(&outbuf, CRLF))
    exit(1);
}

void respond(const char* tagstr, const char* msg)
{
  respond_start(tagstr);
  respond_str(msg);
  respond_end();
}

#if 0
static int isctl(char ch) { return (ch > 0x1f) && (ch < 0x7f); }
static int isquotedspecial(char ch) { return (ch == QUOTE) || (ch == ESCAPE); }

static int isatomspecial(char ch)
{
  switch (ch) {
  case '(':
  case ')':
  case '{':
  case ' ':
  case '%':
  case '*':
    return 0;
  }
  return !isctl(ch) && !isquotedspecial(ch);
}

static int isatom(char ch) { return !isatomspecial(ch); }
static int istag(char ch) { return isatom(ch) && (ch != '+'); }
#endif

/* This parser is rather liberal in what it accepts:
   The IMAP standard mandates seperate character sets for tags, commands,
   unquoted strings.  Since they all must be seperated by spaces, this
   parser allows all non-whitespace characters for each of the above. */
static int parse_line(void)
{
#define RESPOND(T,S) do{ respond(T,S); return 0; }while(0)
  unsigned i;
  unsigned len;
  const char* ptr;
  str* arg;
  
  /* Parse out the command tag */
  str_truncate(&tag, 0);
  for (i = 0, ptr = line.s; i < line.len && isspace(*ptr); ++i, ++ptr) ;
  for (; i < line.len && !isspace(*ptr); ++i, ++ptr)
    str_catc(&tag, line.s[i]);
  if (!tag.len) RESPOND(NOTAG, "BAD Syntax error");
  if (i >= line.len) RESPOND(0, "BAD Syntax error");

  /* Parse out the command itself */
  str_truncate(&cmd, 0);
  for (; i < line.len && isspace(*ptr); ++i, ++ptr) ;
  for (; i < line.len && !isspace(*ptr); ++i, ++ptr)
    str_catc(&cmd, line.s[i]);
  if (!cmd.len) RESPOND(0, "BAD Syntax error");

  /* Parse out the command-line args */
  for (line_argc = 0; line_argc < MAX_ARGC; ++line_argc) {
    arg = &line_args[line_argc];
    str_truncate(arg, 0);
    for (; i < line.len && isspace(*ptr); ++i, ++ptr) ;
    if (i >= line.len) break;

    switch (*ptr) {
    case LBRACE:
      /* Handle a string literal */
      ++i, ++ptr;
      if (!isdigit(*ptr)) RESPOND(0, "BAD Syntax error: missing integer");
      for (len = 0; i < line.len && *ptr != RBRACE; ++i, ++ptr) {
	if (!isdigit(*ptr))
	  RESPOND(0, "BAD Syntax error: invalid integer");
	len = len * 10 + *ptr - '0';
      }
      ++i, ++ptr;
      if (*ptr != 0) RESPOND(0, "BAD Syntax error: missing LF after integer");
      str_ready(arg, len);
      respond(CONT, "OK");
      if (len > 0)
	ibuf_read(&inbuf, arg->s, len);
      arg->s[arg->len = len] = 0;
      ibuf_getstr_crlf(&inbuf, &line);
      i = 0;
      ptr = line.s;
      break;

    case QUOTE:
      /* Handle a quoted string */
      for (++i, ++ptr; i < line.len && *ptr != QUOTE; ++i, ++ptr) {
	if (*ptr == ESCAPE) {
	  if (++i >= line.len) break;
	  ++ptr;
	}
	str_catc(arg, *ptr);
      }
      if (i >= line.len || *ptr != QUOTE)
	RESPOND(0, "BAD Syntax error: unterminated quoted string");
      ++i, ++ptr;
      break;

    default:
      /* Normal case is very simple */
      for (; i < line.len && !isspace(*ptr); ++i, ++ptr)
	str_catc(arg, *ptr);
    }
  }
  for (; i < line.len && isspace(*ptr); ++i, ++ptr) ;
  if (i < line.len) RESPOND(0, "BAD Too many command arguments");
  return 1;
}

void cmd_noop(void)
{
  respond(0, "OK NOOP completed");
}

void cmd_logout(void)
{
  respond(NOTAG, "Logging out");
  respond(0, "OK LOGOUT completed");
  exit(0);
}

void cmd_capability(void)
{
  respond_start(NOTAG);
  respond_str("CAPABILITY IMAP4rev1 ");
  respond_str(capability);
  respond_end();
  respond(0, "OK CAPABILITY completed");
}

void do_exec(void)
{
  if (!cvm_setugid())
    respond(0, "NO Internal error: could not set UID/GID");
  else if (!cvm_setenv() ||
	   (cvm_fact_mailbox != 0 &&
	    setenv("MAILDIR", cvm_fact_mailbox, 1) == -1) ||
	   setenv("IMAPLOGINTAG", tag.s, 1) == -1 ||
	   setenv("AUTHENTICATED", cvm_fact_username, 1) == -1)
    respond(0, "NO Internal error: could not set environment");
  else {
    alarm(0);
    execvp(nextcmd[0], nextcmd);
    respond(0, "NO Could not execute second stage");
  }
  exit(1);
}

void cmd_login(int argc, str* argv)
{
  int cr;
  if (argc != 2)
    respond(0, "BAD LOGIN command requires exactly two arguments");
  else {
    if ((cr = cvm_authenticate_password(cvm, argv[0].s, domain,
					argv[1].s, 1)) == 0)
      do_exec();
    else
      respond(0, "NO LOGIN failed");
  }
}

void cmd_authenticate(int argc, str* argv)
{
  int i;
  if (argc == 1)
    i = sasl_auth2(&saslauth, argv[0].s, 0);
  else if (argc == 2)
    i = sasl_auth2(&saslauth, argv[0].s, argv[1].s);
  else {
    respond(0, "BAD AUTHENTICATE command requires only one or two arguments");
    return;
  }
  if (i == 0)
    do_exec();
  respond_start(0);
  respond_str("NO AUTHENTICATE failed: ");
  respond_str(sasl_auth_msg(&i));
  respond_end();
}

struct command
{
  const char* name;
  void (*fn0)(void);
  void (*fn1)(int argc, str* argv);
};

struct command commands[] = {
  { "CAPABILITY",  cmd_capability, 0 },
  { "NOOP",        cmd_noop,       0 },
  { "LOGOUT",      cmd_logout,     0 },
  { "LOGIN",       0, cmd_login },
  { "AUTHENTICATE", 0, cmd_authenticate },
  { 0, 0, 0 }
};

static void dispatch_line(void)
{
  struct command* c;
  str_upper(&cmd);
  for (c = commands; c->name != 0; ++c) {
    if (str_diffs(&cmd, c->name) == 0) {
      if (line_argc == 0) {
	if (c->fn0 == 0)
	  respond(0, "BAD Syntax error: command requires arguments");
	else
	  c->fn0();
      }
      else {
	if (c->fn1 == 0)
	  respond(0, "BAD Syntax error: command requires no arguments");
	else
	  c->fn1(line_argc, line_args);
      }
      return;
    }
  }
  respond(0, "BAD Unimplemented command");
}

static int startup(int argc, char* argv[])
{
  if (argc < 2) {
    respond(NOTAG, "NO Usage: imapfront-auth imapd [args ...]");
    return 0;
  }
  if ((domain = cvm_ucspi_domain()) == 0)
    domain = "unknown";
  nextcmd = argv + 1;
  if ((cvm = getenv("CVM_SASL_PLAIN")) == 0) {
    respond(NOTAG, "NO $CVM_SASL_PLAIN is not set");
    return 0;
  }
  if (!sasl_auth_init(&saslauth)) {
    respond(NOTAG, "NO Could not initialize SASL AUTH");
    return 0;
  }
  
  if ((capability = getenv("IMAP_CAPABILITY")) == 0 &&
      (capability = getenv("CAPABILITY")) == 0)
    capability = "";
  if (strncasecmp(capability, "IMAP4rev1", 9) == 0) capability += 9;
  while (isspace(*capability)) ++capability;
  
  return 1;
}

const int authenticating = 1;
extern void set_timeout(void);

int main(int argc, char* argv[])
{
  set_timeout();
  if (!startup(argc, argv)) return 0;
  respond(NOTAG, "OK imapfront ready.");
  while (ibuf_getstr_crlf(&inbuf, &line)) {
    if (parse_line())
      dispatch_line();
  }
  if (ibuf_timedout(&inbuf))
    respond(NOTAG, "NO Connection timed out");
  return 0;
}
