#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <msg/msg.h>
#include <unix/sig.h>
#include "mailfront.h"
#include "mailrules.h"
#include <sysdeps.h>
#include "conf_qmail.h"

static str buffer;
static RESPONSE(str,451,"Could not perform string operation.");
static RESPONSE(no_write,451,"Writing data to qmail-queue failed.");
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

void backend_handle_reset(void)
{
  close_qqpipe();
  str_truncate(&buffer, 0);
}

const response* backend_handle_sender(str* sender)
{
  if (!str_catc(&buffer, 'F') ||
      !str_cat(&buffer, sender) ||
      !str_catc(&buffer, 0)) return &resp_str;
  return 0;
}

const response* backend_handle_recipient(str* recipient)
{
  if (!str_catc(&buffer, 'T') ||
      !str_cat(&buffer, recipient) ||
      !str_catc(&buffer, 0)) return &resp_str;
  return 0;
}

const response* backend_handle_data_start(void)
{
  static RESPONSE(no_pipe,451,"Could not open pipe to qmail-queue.");
  static RESPONSE(no_fork,451,"Could not start qmail-queue.");
  static RESPONSE(no_chdir,451,"Could not change to the qmail directory.");

  int mpipe[2];
  int epipe[2];

  qqargs[0] = rules_getenv("QMAILQUEUE");
  if (qqargs[0] == 0) qqargs[0] = "bin/qmail-queue";

  if (chdir(conf_qmail) == -1) return &resp_no_chdir;

  if (pipe(mpipe) == -1) return &resp_no_pipe;
  if (pipe(epipe) == -1) {
    close(mpipe[0]);
    close(mpipe[1]);
    return &resp_no_pipe;
  }
  sig_pipe_block();

  if ((qqpid = fork()) == -1) {
    close(mpipe[0]); close(mpipe[1]);
    close(epipe[0]); close(epipe[1]);
    return &resp_no_fork;
  }

  if (qqpid == 0) {
    if (!rules_exportenv()) exit(51);
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

void backend_handle_data_bytes(const char* bytes, unsigned long len)
{
  retry_write(qqmpipe, bytes, len);
  databytes += len;
}

static void parse_status(int status, response* resp)
{
  const char* message;
  switch (status) {
  case 11: message = "Address too long."; break;
  case 31: message = "Message refused."; break;
  case 51: message = "Out of memory."; break;
  case 52: message = "Timeout."; break;
  case 53: message = "Write error (queue full?)."; break;
  case 54: message = "Unable to read the message or envelope."; break;
  case 55: message = "Unable to read a configuration file."; break;
  case 56: message = "Network problem."; break;
  case 61: message = "Problem with the qmail home directory."; break;
  case 62: message = "Problem with the qmail queue directory."; break;
  case 63: message = "Problem with queue/pid."; break;
  case 64: message = "Problem with queue/mess."; break;
  case 65: message = "Problem with queue/intd."; break;
  case 66: message = "Problem with queue/todo."; break;
  case 71: message = "Message refused by mail server."; break;
  case 72: message = "Connection to mail server timed out."; break;
  case 73: message = "Connection to mail server rejected."; break;
  case 74: message = "Communication with mail server failed."; break;
  case 81: message = "Internal qmail-queue bug."; break;
  case 91: message = "Envelope format error."; break;
  default:
    message = (status <= 40)
      ? "Permanent qmail-queue failure."
      : "Temporary qmail-queue failure.";
  }
  resp->number = (status <= 40) ? 554 : 451;
  resp->message = message;
}

const response* backend_handle_data_end(void)
{
  static RESPONSE(qq_crashed,451,"qmail-queue crashed.");
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
