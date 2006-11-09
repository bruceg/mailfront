#include <sysdeps.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <misc/misc.h>
#include <msg/msg.h>
#include <unix/sig.h>
#include "mailfront.h"
#include "conf_qmail.c"

static RESPONSE(no_write,451,"4.3.0 Writing data to qmail-queue failed.");
static RESPONSE(no_pipe,451,"4.3.0 Could not open pipe to qmail-queue.");
static RESPONSE(no_fork,451,"4.3.0 Could not start qmail-queue.");
static RESPONSE(no_chdir,451,"4.3.0 Could not change to the qmail directory.");
static RESPONSE(qq_crashed,451,"4.3.0 qmail-queue crashed.");

static str buffer;

static const response* reset(void)
{
  str_truncate(&buffer, 0);
  return 0;
}

static const response* do_sender(str* sender)
{
  if (!str_catc(&buffer, 'F') ||
      !str_cat(&buffer, sender) ||
      !str_catc(&buffer, 0)) return &resp_oom;
  return 0;
}

static const response* do_recipient(str* recipient)
{
  if (!str_catc(&buffer, 'T') ||
      !str_cat(&buffer, recipient) ||
      !str_catc(&buffer, 0)) return &resp_oom;
  return 0;
}

static const response* data_start(int fd)
{
  const char* qh;

  if ((qh = session_getenv("QMAILHOME")) == 0)
    qh = conf_qmail;
  if (chdir(qh) == -1) return &resp_no_chdir;

  return 0;
  (void)fd;
}

static int retry_write(int fd, const char* bytes, unsigned long len)
{
  while (len) {
    unsigned long written = write(fd, bytes, len);
    if (written == (unsigned long)-1) return 0;
    len -= written;
    bytes += written;
  }
  return 1;
}

static void parse_status(int status, response* resp)
{
  char var[20];
  const char* message;
  resp->number = (status <= 40 && status >= 11) ? 554 : 451;
  memcpy(var, "QQERRMSG_", 9);
  strcpy(var+9, utoa(status));
  if ((message = session_getenv(var)) == 0) {
    switch (status) {
    case 11: message = "5.1.3 Address too long."; break;
    case 31: message = "5.3.0 Message refused."; break;
    case 51: message = "4.3.0 Out of memory."; break;
    case 52: message = "4.3.0 Timeout."; break;
    case 53: message = "4.3.0 Write error (queue full?)."; break;
    case 54: message = "4.3.0 Unable to read the message or envelope."; break;
    case 55: message = "4.3.0 Unable to read a configuration file."; break;
    case 56: message = "4.3.0 Network problem."; break;
    case 61: message = "4.3.0 Problem with the qmail home directory."; break;
    case 62: message = "4.3.0 Problem with the qmail queue directory."; break;
    case 63: message = "4.3.0 Problem with queue/pid."; break;
    case 64: message = "4.3.0 Problem with queue/mess."; break;
    case 65: message = "4.3.0 Problem with queue/intd."; break;
    case 66: message = "4.3.0 Problem with queue/todo."; break;
    case 71: message = "4.3.0 Message refused by mail server."; break;
    case 72: message = "4.3.0 Connection to mail server timed out."; break;
    case 73: message = "4.3.0 Connection to mail server rejected."; break;
    case 74: message = "4.3.0 Communication with mail server failed."; break;
    case 81: message = "4.3.0 Internal qmail-queue bug."; break;
    case 91: message = "4.3.0 Envelope format error."; break;
    default:
      message = (resp->number >= 500)
	? "5.3.0 Message rejected by qmail-queue."
	: "4.3.0 Temporary qmail-queue failure.";
    }
  }
  resp->message = message;
}

static const response* message_end(int fd)
{
  static response resp;
  const char* qqargs[2] = { 0, 0 };
  int qqpid = -1;
  int epipe[2];
  int status;
  struct stat st;

  if (lseek(fd, 0, SEEK_SET) != 0)
    return &resp_internal;
  if ((qqargs[0] = session_getenv("QMAILQUEUE")) == 0)
    qqargs[0] = "bin/qmail-queue";

  if (pipe(epipe) == -1) return &resp_no_pipe;
  sig_pipe_block();

  if ((qqpid = fork()) == -1) {
    close(epipe[0]); close(epipe[1]);
    return &resp_no_fork;
  }

  if (qqpid == 0) {
    if (!session_exportenv()) exit(51);
    close(epipe[1]);
    if (dup2(fd, 0) == -1) exit(120);
    if (dup2(epipe[0], 1) == -1) exit(120);
    close(epipe[0]);
    execvp(qqargs[0], (char**)qqargs);
    exit(120);
  }

  close(epipe[0]);
  if (!retry_write(epipe[1], buffer.s, buffer.len+1)) return &resp_no_write;
  close(epipe[1]);

  if (waitpid(qqpid, &status, WUNTRACED) == -1) return &resp_qq_crashed;
  if (!WIFEXITED(status)) return &resp_qq_crashed;

  if ((status = WEXITSTATUS(status)) != 0)
    parse_status(status, &resp);
  else {
    fstat(fd, &st);
    str_copys(&buffer, "2.6.0 Accepted message qp ");
    str_catu(&buffer, qqpid);
    str_cats(&buffer, " bytes ");
    str_catu(&buffer, st.st_size);
    msg1(buffer.s);
    resp.number = 250;
    resp.message = buffer.s;
  }
  return &resp;
  (void)fd;
}

struct plugin backend = {
  .reset = reset,
  .sender = do_sender,
  .recipient = do_recipient,
  .data_start = data_start,
  .message_end = message_end,
};
