/* pop3front-main.c -- POP3 main program
 * Copyright (C) 2001  Bruce Guenter <bruceg@em.ca> or FutureQuest, Inc.
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
#include "direntry.h"
#include "iobuf/iobuf.h"
#include "str/str.h"
#include "pop3.h"

typedef struct
{
  const char* filename;
  unsigned long size;
  int read;
  int deleted;
} msg;

#define MSG_DELETED ((unsigned long)-1)

const char program[] = "pop3front-maildir";

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
  struct stat s;
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
    if (stat(tmp.s, &s) == -1) continue;
    if (!S_ISREG(s.st_mode)) continue;
    if (!str_cat(list, &tmp)) return 0;
    if (!str_catc(list, 0)) return 0;
    msg_bytes += s.st_size;
    ++count;
    ++msg_count;
  }
  closedir(dir);
  *countptr = count;
  return 1;
}

static void make_msg(msg* msg, const char* filename)
{
  struct stat s;
  const char* c;
  msg->filename = filename;
  if (stat(filename, &s) == -1)
    msg->size = 0, msg->deleted = 1;
  else
    msg->size = s.st_size, msg->deleted = 0;
  msg->read = 0;
  if ((c = strchr(filename, ':')) != 0)
    if (c[1] == '2' && c[2] == ',')
      if (strchr(c+3, 'S') != 0)
	msg->read = 1;
}

static int fn_compare(const str_sortentry* a, const str_sortentry* b)
{
  int at = atoi(a->str+4);
  int bt = atoi(b->str+4);
  return at - bt;
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
    msg* msg = &msgs[i];
    if (!msg->deleted) {
      const char* fn = msg->filename + 4;
      const char* end;
      obuf_putu(&outbuf, i+1);
      obuf_putc(&outbuf, SPACE);
      if ((end = strchr(fn, ':')) == 0)
	obuf_puts(&outbuf, fn);
      else
	obuf_write(&outbuf, fn, end - fn);
      obuf_puts(&outbuf, CRLF);
    }
  }
  respond(".");
}

static void cmd_uidl_one(const str* arg)
{
  long i;
  const char* end;
  const char* fn;
  if ((i = msgnum(arg)) == 0) return;
  fn = msgs[i-1].filename + 4;
  if ((end = strchr(fn, ':')) == 0)
    end = fn + strlen(fn);
  if (!str_copys(&tmp, "+OK ") ||
      !str_catu(&tmp, i) ||
      !str_catc(&tmp, SPACE) ||
      !str_catb(&tmp, fn, end-fn))
    respond(err_internal);
  else
    respond(tmp.s);
}

command commands[] = {
  { "DELE", 0,        cmd_dele },
  { "LAST", cmd_last, 0 },
  { "LIST", cmd_list, cmd_list_one },
  { "NOOP", cmd_noop, 0 },
  { "QUIT", cmd_quit, 0 },
  { "RETR", 0,        cmd_top },
  { "RSET", cmd_rset, 0 },
  { "STAT", cmd_stat, 0 },
  { "TOP",  0,        cmd_top },
  { "UIDL", cmd_uidl, cmd_uidl_one },
  { 0,      0,        0 }
};

int startup(int argc, char* argv[])
{
  const char* tmp;
  static const char usage[] = "usage: pop3front-main [default-maildir]\n";
  if (argc > 2) {
    obuf_putsflush(&errbuf, usage);
    return 0;
  }

  if ((tmp = getenv("MAX_MESSAGES")) != 0)     max_count = atol(tmp);
  if ((tmp = getenv("MAX_CUR_MESSAGES")) != 0) max_cur_count = atol(tmp);
  if ((tmp = getenv("MAX_NEW_MESSAGES")) != 0) max_new_count = atol(tmp);
  
  if ((tmp = getenv("MAILBOX")) == 0) {
    if (argc < 2) {
      obuf_putsflush(&errbuf, "pop3front-main: Mailbox not specified\n");
      return 0;
    }
    tmp = argv[1];
  }
  if (chdir(tmp) == -1) {
    respond("-ERR Could not chdir to maildir");
    return 0;
  }
  if (!scan_maildir()) {
    respond("-ERR Could not access maildir");
    return 0;
  }
  return 1;
}
