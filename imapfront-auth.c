/* imapfront-auth.c - IMAP authentication front-end
 * Copyright (C) 2002  Bruce Guenter <bruceg@em.ca> or FutureQuest, Inc.
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
#include "setenv.h"
#include "sasl-auth.h"
#include "cvm/client.h"
#include "iobuf/iobuf.h"
#include "str/str.h"

const char program[] = "imapfront-auth";
const int msg_show_pid = 1;

#define MAX_ARGC 16
#define QUOTE '"'
#define ESCAPE '\\'

static const char* capability;
static const char* cvm;
static const char* domain;
static char** nextcmd;
static unsigned timeout;
static str tag;
static str line;
static str cmd;
static str args[MAX_ARGC];
static int argc;

void log_start(const char* tag)
{
  obuf_puts(&errbuf, program);
  obuf_putc(&errbuf, '[');
  obuf_putu(&errbuf, getpid());
  obuf_puts(&errbuf, "]: ");
  if (tag) {
    obuf_puts(&errbuf, tag);
    obuf_putc(&errbuf, ' ');
  }
}

void log_str(const char* msg) { obuf_puts(&errbuf, msg); }
void log_end(void) { obuf_putsflush(&errbuf, "\n"); }

void log(const char* tag, const char* msg)
{
  log_start(tag);
  log_str(msg);
  log_end();
}

void respond_start(int tagged)
{
  const char* tagstr;
  if (tagged) tagstr = tag.s;
  else tagstr = "*";
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

void respond(int tagged, const char* msg)
{
  respond_start(tagged);
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
  const char* ptr;
  str* arg;
  
  /* Parse out the command tag */
  str_truncate(&tag, 0);
  for (i = 0, ptr = line.s; i < line.len && isspace(*ptr); ++i, ++ptr) ;
  for (; i < line.len && !isspace(*ptr); ++i, ++ptr)
    str_catc(&tag, line.s[i]);
  if (!tag.len) RESPOND(0, "BAD Syntax error");
  if (i >= line.len) RESPOND(1, "BAD Syntax error");

  /* Parse out the command itself */
  str_truncate(&cmd, 0);
  for (; i < line.len && isspace(*ptr); ++i, ++ptr) ;
  for (; i < line.len && !isspace(*ptr); ++i, ++ptr)
    str_catc(&cmd, line.s[i]);
  if (!cmd.len) RESPOND(1, "BAD Syntax error");

  /* Parse out the command-line args */
  for (argc = 0; argc < MAX_ARGC; ++argc) {
    arg = &args[argc];
    str_truncate(arg, 0);
    for (; i < line.len && isspace(*ptr); ++i, ++ptr) ;
    if (i >= line.len) break;
    if (*ptr == QUOTE) {
      for (++i, ++ptr; i < line.len && *ptr != QUOTE; ++i, ++ptr) {
	if (*ptr == ESCAPE) {
	  if (++i >= line.len) break;
	  ++ptr;
	}
	str_catc(arg, *ptr);
      }
      if (i >= line.len || *ptr != QUOTE)
	RESPOND(1, "BAD Syntax error: unterminated quoted string");
      ++i, ++ptr;
    }
    else
      for (; i < line.len && !isspace(*ptr); ++i, ++ptr)
	str_catc(arg, *ptr);
  }
  for (; i < line.len && isspace(*ptr); ++i, ++ptr) ;
  if (i < line.len) RESPOND(1, "BAD Too many command arguments");
  return 1;
}

void cmd_noop(void)
{
  respond(1, "OK NOOP completed");
}

void cmd_logout(void)
{
  respond(0, "Logging out");
  respond(1, "OK LOGOUT completed");
  exit(0);
}

void cmd_capability(void)
{
  respond_start(0);
  respond_str("CAPABILITY IMAP4rev1 ");
  respond_str(capability);
  respond_end();
  respond(1, "OK CAPABILITY completed");
}

void do_exec(void)
{
  if (!cvm_setugid())
    respond(1, "NO Internal error: could not set UID/GID");
  else if (!cvm_setenv() ||
	   setenv("IMAPLOGINTAG", tag.s, 1) == -1 ||
	   setenv("AUTHENTICATED", cvm_fact_username, 1) == -1)
    respond(1, "NO Internal error: could not set environment");
  else {
    execvp(nextcmd[0], nextcmd);
    respond(1, "NO Could not execute second stage");
  }
  exit(1);
}

void cmd_login(int argc, str* argv)
{
  int cr;
  if (argc != 2)
    respond(1, "BAD LOGIN command requires exactly two arguments");
  else {
    const char* credentials[2] = { argv[1].s, 0 };
    if ((cr = cvm_authenticate(cvm, argv[0].s, domain, credentials, 1)) == 0)
      do_exec();
    else
      respond(1, "NO LOGIN failed");
  }
}

void cmd_authenticate(int argc, str* argv)
{
  int i;
  if (argc == 1)      i = sasl_auth2("+ ", argv[0].s, 0);
  else if (argc == 2) i = sasl_auth2("+ ", argv[0].s, argv[1].s);
  else {
    respond(1, "BAD AUTHENTICATE command requires only one or two arguments");
    return;
  }
  if (i == 0)
    do_exec();
  respond_start(1);
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
      if (argc == 0) {
	if (c->fn0 == 0)
	  respond(1, "BAD Syntax error: command requires arguments");
	else
	  c->fn0();
      }
      else {
	if (c->fn1 == 0)
	  respond(1, "BAD Syntax error: command requires no arguments");
	else
	  c->fn1(argc, args);
      }
      return;
    }
  }
  respond(1, "BAD Unimplemented command");
}

static int startup(int argc, char* argv[])
{
  if (argc < 2) {
    respond(0, "NO Usage: imapfront-auth imapd [args ...]");
    return 0;
  }
  domain = getenv("TCPLOCALHOST");
  nextcmd = argv + 1;
  if ((cvm = getenv("CVM_SASL_PLAIN")) == 0) {
    respond(0, "NO $CVM_SASL_PLAIN is not set");
    return 0;
  }
  if (!sasl_auth_init()) {
    respond(0, "NO Could not initialize SASL AUTH");
    return 0;
  }
  
  if ((capability = getenv("IMAP_CAPABILITY")) == 0 &&
      (capability = getenv("CAPABILITY")) == 0)
    capability = "";
  if (strncasecmp(capability, "IMAP4rev1", 9) == 0) capability += 9;
  while (isspace(*capability)) ++capability;
  
  return 1;
}

int main(int argc, char* argv[])
{
  const char* tmp;

  timeout = 0;
  if ((tmp = getenv("TIMEOUT")) != 0) timeout = strtoul(tmp, 0, 10);
  if (timeout <= 0) timeout = 1200;
  inbuf.io.timeout = timeout * 1000;
  outbuf.io.timeout = timeout * 1000;

  if (!startup(argc, argv)) return 0;
  respond(0, "imapfront ready.");
  while (ibuf_getstr_crlf(&inbuf, &line)) {
    if (parse_line())
      dispatch_line();
  }
  if (ibuf_timedout(&inbuf))
    respond(0, "NO Connection timed out");
  return 0;
}
