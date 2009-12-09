/* pop3front-maildir.c -- POP3 main program
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sysdeps.h>
#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/str.h>
#include "pop3.h"

typedef struct
{
  const char* filename;
  unsigned long size;
  int read;
  int deleted;
  int uid_len;
} msg;

#define MSG_DELETED ((unsigned long)-1)

const char program[] = "pop3front-maildir";
const int authenticating = 0;

static str msg_filenames;
static msg* msgs;

static long max_count;
static long max_new_count;
static long max_cur_count;

static long new_count;
static long cur_count;
static long del_count;
static long msg_count;
static long msg_bytes;
static long del_bytes;
static str tmp;

static long scan_dir(const char* subdir, str* list, long* countptr, long max)
{
  DIR* dir;
  direntry* entry;
  long count;
  
  if ((dir = opendir(subdir)) == 0) return 0;
  count = 0;
  while (!(max_count > 0 && msg_count >= max_count) &&
	 !(max > 0 && count >= max) &&
	 (entry = readdir(dir)) != 0) {
    if (entry->d_name[0] == '.') continue;
    if (!str_copys(&tmp, subdir)) return 0;
    if (!str_catc(&tmp, '/')) return 0;
    if (!str_cats(&tmp, entry->d_name)) return 0;
    if (!str_cat(list, &tmp)) return 0;
    if (!str_catc(list, 0)) return 0;
    ++count;
    ++msg_count;
  }
  closedir(dir);
  *countptr = count;
  return 1;
}

static void make_msg(msg* m, const char* filename)
{
  struct stat s;
  const char* c;
  const char* base = filename + 4;

  m->filename = filename;
  m->uid_len = strlen(base);
  m->read = 0;
  if ((c = memchr(base, ':', m->uid_len)) != 0) {
    if (c[1] == '2' && c[2] == ',')
      if (strchr(c+3, 'S') != 0)
	m->read = 1;
    m->uid_len = c - base;
  }

  if (stat(filename, &s) == -1)
    m->size = 0, m->deleted = 1;
  else
    m->size = s.st_size, m->deleted = 0;
  msg_bytes += m->size;
}

static int fn_compare(const str_sortentry* a, const str_sortentry* b)
{
  if (a->str[4] < '0'
      || a->str[4] > '9'
      || b->str[4] < '0'
      || b->str[4] > '9')
    return strcmp(a->str+4, b->str+4);
  else {
    int at = atoi(a->str+4);
    int bt = atoi(b->str+4);
    return at - bt;
  }
}

static int scan_maildir(void)
{
  long pos;
  long i;

  msg_bytes = 0;
  if (!str_truncate(&msg_filenames, 0)) return 0;
  msg_count = 0;
  if (!scan_dir("cur", &msg_filenames, &cur_count, max_cur_count)) return 0;
  if (!scan_dir("new", &msg_filenames, &new_count, max_new_count)) return 0;
  if (msg_count == 0) return 1;
  if (!str_sort(&msg_filenames, 0, -1, fn_compare)) return 0;

  del_count = 0;
  del_bytes = 0;
  
  if (msgs != 0) free(msgs);
  if ((msgs = malloc(msg_count * sizeof msgs[0])) == 0) return 0;

  for (i = pos = 0; i < msg_count; pos += strlen(msg_filenames.s+pos)+1, ++i)
    make_msg(&msgs[i], msg_filenames.s + pos);

  return 1;
}

static int msgnum_check(long i)
{
  if (i > msg_count)
    respond("-ERR Message number out of range");
  else if (msgs[i-1].deleted)
    respond("-ERR Message was deleted");
  else
    return 1;
  return 0;
}

static long msgnum(const str* arg)
{
  long i;
  char* end;
  if ((i = strtol(arg->s, &end, 10)) <= 0 || *end != 0)
    respond(err_syntax);
  else if (msgnum_check(i))
    return i;
  return 0;
}

static void dump_msg(long num, long bodylines)
{
  ibuf in;
  static char buf[4096];
  int in_header;		/* True until a blank line is seen */
  int sol;			/* True if at start of line */
  
  if (!msgnum_check(num)) return;

  if (!ibuf_open(&in, msgs[num-1].filename, 0))
    return respond("-ERR Could not open that message");
  respond(ok);

  sol = in_header = 1;
  while (ibuf_read(&in, buf, sizeof buf) || in.count) {
    const char* ptr = buf;
    const char* end = buf + in.count;
    while (ptr < end) {
      const char* lfptr;
      if (sol) {
	if (!in_header)
	  if (--bodylines < 0) break;
	if (*ptr == '.')
	  obuf_putc(&outbuf, '.');
      }
      if ((lfptr = memchr(ptr, LF, end-ptr)) == 0) {
	obuf_write(&outbuf, ptr, end-ptr);
	ptr = end;
	sol = 0;
      }
      else {
	if (in_header && lfptr == ptr)
	  in_header = 0;
	obuf_write(&outbuf, ptr, lfptr-ptr);
	obuf_puts(&outbuf, CRLF);
	ptr = lfptr + 1;
	sol = 1;
      }
    }
  }
  ibuf_close(&in);
  obuf_puts(&outbuf, CRLF);
  respond(".");
}

/* Mark a maildir filename with the named flag */
static int add_flag(str* fn, char flag)
{
  int c;
  /* If the filename has no flags, append them */
  if ((c = str_findfirst(fn, ':')) == -1) {
    if (!str_cats(fn, ":2,")) return 0;
  }
  else {
    /* If it has a colon (start of flags), see if they are a type we
     * recognize, and bail out if they aren't */
    if (fn->s[c+1] != '2' || fn->s[c+2] != ',') return 1;
    /* Scan through the flag characters and return success
     * if the message is already marked with the flag */
    if (strchr(fn->s+c+3, flag) != 0) return 1;
  }
  return str_catc(fn, flag);
}

/* Commands ******************************************************************/
static void cmd_dele(const str* arg)
{
  long i;
  if ((i = msgnum(arg)) == 0) return;
  --i;
  del_bytes += msgs[i].size;
  del_count++;
  msgs[i].deleted = 1;
  respond(ok);
}

static void cmd_last(void)
{
  long last;
  long i;
  for (last = i = 0; i < msg_count; i++)
    if (msgs[i].read)
      last = i + 1;
  if (!str_copys(&tmp, "+OK ") ||
      !str_cati(&tmp, last))
    respond(err_internal);
  respond(tmp.s);
}

static void cmd_list(void)
{
  long i;
  respond(ok);
  for (i = 0; i < msg_count; i++) {
    if (!msgs[i].deleted) {
      obuf_putu(&outbuf, i+1);
      obuf_putc(&outbuf, SPACE);
      obuf_putu(&outbuf, msgs[i].size);
      obuf_puts(&outbuf, CRLF);
    }
  }
  respond(".");
}

static void cmd_list_one(const str* arg)
{
  long i;
  if ((i = msgnum(arg)) == 0) return;
  if (!str_copys(&tmp, "+OK ") ||
      !str_catu(&tmp, i) ||
      !str_catc(&tmp, SPACE) ||
      !str_catu(&tmp, msgs[i-1].size))
    respond(err_internal);
  else
    respond(tmp.s);
}

static void cmd_noop(void)
{
  respond(ok);
}

static void cmd_quit(void)
{
  long i;
  for (i = 0; i < msg_count; i++) {
    const char* fn = msgs[i].filename;
    if (msgs[i].deleted)
      unlink(fn);
    /* Logic: 
     * 1. move all messages into "cur"
     * 2. tag all read messages without flags with a read flag (:2,S)
     * Note: no real opportunity to report errors,
     * so just continue when we hit one.
     */
    else {
      if (!str_copys(&tmp, "cur/")) continue;
      if (!str_cats(&tmp, fn+4)) continue;
      if (msgs[i].read && !add_flag(&tmp, 'S')) continue;
      rename(fn, tmp.s);
    }
  }
  respond(ok);
  exit(0);
}

static void cmd_rset(void)
{
  if (!scan_maildir()) {
    respond(err_internal);
    exit(1);
  }
  respond(ok);
}

static void cmd_stat(void)
{
  if (!str_copys(&tmp, "+OK ") ||
      !str_catu(&tmp, msg_count - del_count) ||
      !str_catc(&tmp, SPACE) ||
      !str_catu(&tmp, msg_bytes - del_bytes))
    respond(err_internal);
  else
    respond(tmp.s);
}

static void cmd_top(const str* arg)
{
  long num;
  long lines;
  char* end;

  if ((num = strtol(arg->s, &end, 10)) <= 0) return respond(err_syntax);
  while (*end == SPACE) ++end;
  if (*end == 0) {
    msgs[num-1].read = 1;
    return dump_msg(num, LONG_MAX);
  }
  if ((lines = strtol(end, &end, 10)) < 0 || *end != 0)
    return respond(err_syntax);
  dump_msg(num, lines);
}

static void cmd_uidl(void)
{
  long i;
  respond(ok);
  for (i = 0; i < msg_count; i++) {
    msg* m = &msgs[i];
    if (!m->deleted) {
      const char* fn = m->filename + 4;
      obuf_putu(&outbuf, i+1);
      obuf_putc(&outbuf, SPACE);
      obuf_write(&outbuf, fn, m->uid_len);
      obuf_puts(&outbuf, CRLF);
    }
  }
  respond(".");
}

static void cmd_uidl_one(const str* arg)
{
  long i;
  const char* fn;
  if ((i = msgnum(arg)) == 0) return;
  msg* m = &msgs[i-1];
  fn = m->filename + 4;
  if (!str_copys(&tmp, "+OK ") ||
      !str_catu(&tmp, i) ||
      !str_catc(&tmp, SPACE) ||
      !str_catb(&tmp, fn, m->uid_len))
    respond(err_internal);
  else
    respond(tmp.s);
}

command commands[] = {
  { "CAPA", cmd_capa, 0,            0 },
  { "DELE", 0,        cmd_dele,     0 },
  { "LAST", cmd_last, 0,            0 },
  { "LIST", cmd_list, cmd_list_one, 0 },
  { "NOOP", cmd_noop, 0,            0 },
  { "QUIT", cmd_quit, 0,            0 },
  { "RETR", 0,        cmd_top,      0 },
  { "RSET", cmd_rset, 0,            0 },
  { "STAT", cmd_stat, 0,            0 },
  { "TOP",  0,        cmd_top,      0 },
  { "UIDL", cmd_uidl, cmd_uidl_one, 0 },
  { 0,      0,        0,            0 }
};

extern void report_io_bytes(void);

int startup(int argc, char* argv[])
{
  const char* env;
  if (argc > 2) {
    msg3("usage: ", program, " [default-maildir]");
    return 0;
  }

  if ((env = getenv("MAX_MESSAGES")) != 0)     max_count = atol(env);
  if ((env = getenv("MAX_CUR_MESSAGES")) != 0) max_cur_count = atol(env);
  if ((env = getenv("MAX_NEW_MESSAGES")) != 0) max_new_count = atol(env);
  
  if ((env = getenv("MAILBOX")) == 0) {
    if (argc < 2) {
      error1("Mailbox not specified");
      return 0;
    }
    env = argv[1];
  }
  if (chdir(env) == -1) {
    respond("-ERR Could not chdir to maildir");
    return 0;
  }
  if (!scan_maildir()) {
    respond("-ERR Could not access maildir");
    return 0;
  }
  atexit(report_io_bytes);
  return 1;
}
