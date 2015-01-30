#include "mailfront.h"
#include <stdlib.h>
#include <unistd.h>
#include <bglibs/iobuf.h>

static RESPONSE(start, 220, "2.0.0 Ready to start TLS");
static RESPONSE(earlytalker, 500, "5.5.1 Unexpected pipelined commands following STARTTLS");

static int tls_available = 0;

static const response* init(void)
{
  tls_available = getenv("UCSPITLS") != 0;
  return 0;
}

static const response* helo(str* hostname, str* capabilities)
{
  if (tls_available)
    if (!str_cats(capabilities, "STARTTLS\n")) return &resp_oom;
  return 0;
  (void)hostname;
}

static int starttls(void)
{
  int fd;
  char *fdstr;
  int extrachars = 0;
  char c;

  /* STARTTLS must be the last command in a pipeline, otherwise we can
   * create a security risk (see CVE-2011-0411).  Close input and
   * check for any extra pipelined commands, so we can give an error
   * message.  Note that this will cause an error on the filehandle,
   * since we have closed it. */
  close(0);

  while (!ibuf_eof(&inbuf) && !ibuf_error(&inbuf)) {
    if (ibuf_getc(&inbuf, &c))
      ++extrachars;
  }

  if (!(fdstr=getenv("SSLCTLFD")))
    return 0;
  if ((fd = atoi(fdstr)) <= 0)
    return 0;
  if (write(fd, "y", 1) < 1)
    return 0;

  if (!(fdstr=getenv("SSLREADFD")))
    return 0;
  if ((fd = atoi(fdstr)) <= 0)
    return 0;
  if (dup2(fd, 0) != 0)
    return 0;

  if (!(fdstr=getenv("SSLWRITEFD")))
    return 0;
  if ((fd = atoi(fdstr)) <= 0)
    return 0;
  if (dup2(fd, 1) != 1)
    return 0;

  /* Re-initialize stdin and clear input buffer */
  ibuf_init(&inbuf,0,0,IOBUF_NEEDSCLOSE, 4096);

  if (extrachars)
    respond(&resp_earlytalker);

  return 1;
}

static int enabled(void)
{
  return tls_available;
}

static int cmd_STARTTLS(void)
{
  if (!respond(&resp_start))
    return 0;

  if (!starttls())
    return 0;

  tls_available = 0;
  session_setnum("tls_state", 1);

  /* Remove UCSPITLS from environment to indicate it no longer available. */
  unsetenv("UCSPITLS");
  return 1;
}

static const struct command commands[] = {
  { "STARTTLS", .fn_enabled = enabled, .fn_noparam = cmd_STARTTLS },
  { .name = 0 }
};

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .commands = commands,
  .init = init,
  .helo = helo,
};
