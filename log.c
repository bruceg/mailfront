#include <string.h>
#include <unistd.h>
#include "log.h"
#include "iobuf/iobuf.h"

static int pid = 0;

void log_s(const char* str)
{
  obuf_puts(&errbuf, str);
}

void log_u(unsigned long u)
{
  obuf_putu(&errbuf, u);
}

void log_start(void)
{
  if (pid == 0) pid = getpid();
  log_s(program);
  log_s(" [");
  log_u(pid);
  log_s("]: ");
}

void log_end(void)
{
  log_s("\n");
  obuf_flush(&errbuf);
}

void log1(const char* str)
{
  log_start();
  log_s(str);
  log_end();
}
