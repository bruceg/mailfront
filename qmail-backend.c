#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <msg/msg.h>
#include "mailfront.h"
#include "qmail.h"
#include <sysdeps.h>
#include "conf_qmail.h"

static str buffer;
static const response resp_str = {0,451,"Could not perform string operation."};
static const response resp_no_write = {0,451,"Writing data to qmail-queue failed."};
static unsigned long databytes;

static char* qqargs[2] = { 0, 0 };
static int qqpid = -1;
static int qqepipe = -1;
static int qqmpipe = -1;

static void close_qqpipe(void)
{
  if (qqepipe != -1) close(qqepipe);
  if (qqmpipe != -1) close(qqmpipe);
  qqepipe = qqmpipe = -1;
}

void qmail_reset(void)
{
  close_qqpipe();
  str_truncate(&buffer, 0);
}

const response* qmail_sender(const str* sender)
{
  if (!str_catc(&buffer, 'F') ||
      !str_cat(&buffer, sender) ||
      !str_catc(&buffer, 0)) return &resp_str;
  return 0;
}

const response* qmail_recipient(const str* recipient)
{
  if (!str_catc(&buffer, 'T') ||
      !str_cat(&buffer, recipient) ||
      !str_catc(&buffer, 0)) return &resp_str;
  return 0;
}

const response* qmail_data_start(void)
{
  static const response resp_no_pipe = {0,451,"Could not open pipe to qmail-queue."};
  static const response resp_no_fork = {0,451,"Could not start qmail-queue."};
  static const response resp_no_chdir = {0,451,"Could not change to the qmail directory."};

  int mpipe[2];
  int epipe[2];

  if (qqargs[0] == 0) qqargs[0] = getenv("QMAILQUEUE");
  if (qqargs[0] == 0) qqargs[0] = "bin/qmail-queue";

  if (chdir(conf_qmail) == -1) return &resp_no_chdir;

  if (pipe(mpipe) == -1) return &resp_no_pipe;
  if (pipe(epipe) == -1) {
    close(mpipe[0]);
    close(mpipe[1]);
    return &resp_no_pipe;
  }

  if ((qqpid = fork()) == -1) {
    close(mpipe[0]); close(mpipe[1]);
    close(epipe[0]); close(epipe[1]);
    return &resp_no_fork;
  }

  if (qqpid == 0) {
    close(mpipe[1]);
    close(epipe[1]);
    if (dup2(mpipe[0], 0) == -1) exit(120);
    if (dup2(epipe[0], 1) == -1) exit(120);
    close(mpipe[0]);
    close(epipe[0]);
    execvp(qqargs[0], qqargs);
    exit(120);
  }
  else {
    close(mpipe[0]);
    close(epipe[0]);
    qqmpipe = mpipe[1];
    qqepipe = epipe[1];
  }
  databytes = 0;
  return 0;
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

void qmail_data_bytes(const char* bytes, unsigned long len)
{
  retry_write(qqmpipe, bytes, len);
  databytes += len;
}

static void parse_status(int status, response* resp)
{
  const char* str;
  switch (status) {
  case 11: str = "Address too long."; break;
  case 31: str = "Message refused."; break;
  case 51: str = "Out of memory."; break;
  case 52: str = "Timeout."; break;
  case 53: str = "Write error (queue full?)."; break;
  case 54: str = "Unable to read the message or envelope."; break;
  case 55: str = "Unable to read a configuration file."; break;
  case 56: str = "Network problem."; break;
  case 61: str = "Problem with the qmail home directory."; break;
  case 62: str = "Problem with the qmail queue directory."; break;
  case 63: str = "Problem with queue/pid."; break;
  case 64: str = "Problem with queue/mess."; break;
  case 65: str = "Problem with queue/intd."; break;
  case 66: str = "Problem with queue/todo."; break;
  case 71: str = "Message refused by mail server."; break;
  case 72: str = "Connection to mail server timed out."; break;
  case 73: str = "Connection to mail server rejected."; break;
  case 74: str = "Communication with mail server failed."; break;
  case 81: str = "Internal qmail-queue bug."; break;
  case 91: str = "Envelope format error."; break;
  default:
    str = (status <= 40)
      ? "Permanent qmail-queue failure."
      : "Temporary qmail-queue failure.";
  }
  resp->number = (status <= 40) ? 554 : 451;
  resp->message = str;
}

const response* qmail_data_end(void)
{
  static const response resp_qq_crashed = {0,451,"qmail-queue crashed."};
  static response resp;

  int status;

  close(qqmpipe);
  if (!retry_write(qqepipe, buffer.s, buffer.len+1)) return &resp_no_write;
  close(qqepipe);
  if (waitpid(qqpid, &status, WUNTRACED) == -1) return &resp_qq_crashed;
  if (!WIFEXITED(status)) return &resp_qq_crashed;

  if ((status = WEXITSTATUS(status)) != 0)
    parse_status(status, &resp);
  else {
    str_copys(&buffer, "Accepted message qp ");
    str_catu(&buffer, qqpid);
    str_cats(&buffer, " bytes ");
    str_catu(&buffer, databytes);
    msg1(buffer.s);
    resp.number = 250;
    resp.message = buffer.s;
  }
  return &resp;
}
