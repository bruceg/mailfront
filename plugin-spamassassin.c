#include "mailfront.h"

#include <sysdeps.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <msg/msg.h>
#include <net/resolve.h>
#include <net/socket.h>

static RESPONSE(no_hostname,451,"4.3.0 Could not resolve SpamAssassin hostname");
static RESPONSE(no_scan,451,"4.3.0 Could not SpamAssassin scan message");
static response resp_spam = { 554, 0 };

#define MAX_IPS 16

static str line;

static int try_connect(const ipv4addr* ip, ipv4port port,
		       unsigned long timeout)
{
  int sock;
  if ((sock = socket_tcp()) >= 0) {
    if (socket_connect4_timeout(sock, ip, port, timeout))
      return sock;
    close(sock);
  }
  return -1;
}

static int try_connect_unix(const char* path, unsigned long timeout)
{
  int sock;
  if ((sock = socket_unixstr()) >= 0) {
    if (socket_connectu_timeout(sock, path, timeout))
      return sock;
    close(sock);
  }
  return -1;
}

static unsigned long timeout;
static unsigned long connect_timeout;
static unsigned long send_timeout;
static unsigned long maxsize;

static int scanit(int fd, int fdout, int sock,
		  const struct stat* st)
{
  ibuf netin;
  obuf netout;

  if (ibuf_init(&netin, sock, 0, IOBUF_NEEDSCLOSE, 0)) {
    netin.io.timeout = timeout;
    if (obuf_init(&netout, sock, 0, 0, 0)) {
      netout.io.timeout = send_timeout;
      obuf_putf(&netout,
		"{PROCESS SPAMC/1.2\r\n}"
		"{Content-Length: }lu{\r\n}"
		"{\r\n}",
		st->st_size);
      obuf_copyfromfd(fd, &netout);
      if (obuf_flush(&netout)) {
	if (socket_shutdown(sock, 0, 1)) {
	  if (ibuf_getstr(&netin, &line, LF)) {
	    str_rstrip(&line);
	    if (str_diffs(&line, "SPAMD/1.1 0 EX_OK") == 0) {
	      while (ibuf_getstr(&netin, &line, LF)) {
		str_rstrip(&line);
		if (line.len == 0) {
		  if (ibuf_copytofd(&netin, fdout)) {
		    dup2(fdout, fd);
		    close(fdout);
		    obuf_close(&netout);
		    ibuf_close(&netin);
		    return 1;
		  }
		}
	      }
	    }
	  }
	}
      }
      obuf_close(&netout);
    }
    ibuf_close(&netin);
  }
  return 0;
}

static const response* message_end(int fd)
{
  const char* hostname;
  const char* path;
  const char* tmp;
  ipv4port port;
  ipv4addr ips[MAX_IPS];
  int ip_count;
  int i;
  int offset;
  struct timeval tv;
  int sock;
  struct stat st;
  int fdout;

  hostname = 0;
  if ((path = session_getenv("SPAMD_PATH")) != 0
      || (hostname = session_getenv("SPAMD_HOST")) != 0) {

    if ((tmp = session_getenv("SPAMD_MAXSIZE")) != 0
	&& (maxsize = strtoul(tmp, (char**)&tmp, 10)) != 0
	&& *tmp == 0) {
      if (fstat(fd, &st) != 0)
	return &resp_internal;
      if (st.st_size > (ssize_t)maxsize){
	warn1("SpamAssassin scanning skipped: message larger than maximum");
	return 0;
      }
    }

    if ((tmp = session_getenv("SPAMD_PORT")) == 0
	|| (port = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      port = 783;
    if ((tmp = session_getenv("SPAMD_TIMEOUT")) == 0
	|| (timeout = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      timeout = 5000;
    if ((tmp = session_getenv("SPAMD_CONNECT_TIMEOUT")) == 0
	|| (connect_timeout = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      connect_timeout = timeout;
    if ((tmp = session_getenv("SPAMD_SEND_TIMEOUT")) == 0
	|| (send_timeout = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      send_timeout = timeout;
    if ((ip_count = resolve_ipv4name_n(hostname, ips, MAX_IPS)) <= 0)
      return &resp_no_hostname;

    if ((fdout = scratchfile()) == -1)
      return &resp_internal;

    if (path != 0) {
      if (lseek(fd, 0, SEEK_SET) != 0)
	return &resp_internal;
      if ((sock = try_connect_unix(path, connect_timeout)) >= 0)
	if (scanit(fd, fdout, sock, &st))
	  return 0;
    }
    else {
      gettimeofday(&tv, 0);
      offset = (tv.tv_sec ^ tv.tv_usec) % ip_count;
      for (i = 0; i < ip_count; ++i) {
	const ipv4addr* addr = &ips[(i + offset) % ip_count];
	if (lseek(fd, 0, SEEK_SET) != 0)
	  return &resp_internal;
	if ((sock = try_connect(addr, port, connect_timeout)) < 0)
	  continue;
	if (scanit(fd, fdout, sock, &st))
	  return 0;
      }
    }
  }
  return &resp_no_scan;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = FLAG_NEED_FILE,
  .message_end = message_end,
};
