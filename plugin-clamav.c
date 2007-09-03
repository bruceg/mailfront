#include "mailfront.h"

#include <sysdeps.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <msg/msg.h>
#include <net/resolve.h>
#include <net/socket.h>

static RESPONSE(no_hostname,451,"4.3.0 Could not resolve virus scanner hostname");
static RESPONSE(no_scan,451,"4.3.0 Could not virus scan message");
static response resp_virus = { 554, 0 };

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

static const response* message_end(int fd)
{
  const char* hostname;
  const char* tmp;
  ipv4port cmdport;
  ipv4port dataport;
  ipv4addr ips[MAX_IPS];
  int ip_count;
  int i;
  int offset;
  struct timeval tv;
  int sock;
  unsigned long timeout;
  unsigned long connect_timeout;
  unsigned long send_timeout;
  unsigned long maxsize;
  ibuf netin;
  obuf netout;
  struct stat st;
  
  if ((hostname = session_getenv("CLAMAV_HOST")) != 0
      || (hostname = session_getenv("CLAMD_HOST")) != 0) {

    if ((tmp = session_getenv("CLAMAV_MAXSIZE")) != 0
	&& (maxsize = strtoul(tmp, (char**)&tmp, 10)) != 0
	&& *tmp == 0) {
      if (fstat(fd, &st) != 0)
	return &resp_internal;
      if (st.st_size > (ssize_t)maxsize){
	warn1("ClamAV scanning skipped: message larger than maximum");
	return 0;
      }
    }

    if (((tmp = session_getenv("CLAMAV_PORT")) == 0
	 && (tmp = session_getenv("CLAMD_PORT")) == 0)
	|| (cmdport = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      cmdport = 3310;
    if (((tmp = session_getenv("CLAMAV_TIMEOUT")) == 0
	 && (tmp = session_getenv("CLAMD_TIMEOUT")) == 0)
	|| (timeout = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      timeout = 5000;
    if ((tmp = session_getenv("CLAMAV_CONNECT_TIMEOUT")) == 0
	|| (connect_timeout = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      connect_timeout = timeout;
    if ((tmp = session_getenv("CLAMAV_SEND_TIMEOUT")) == 0
	|| (send_timeout = strtoul(tmp, (char**)&tmp, 10)) == 0
	|| *tmp != 0)
      send_timeout = timeout;
    if ((ip_count = resolve_ipv4name_n(hostname, ips, MAX_IPS)) <= 0)
      return &resp_no_hostname;

    gettimeofday(&tv, 0);
    offset = (tv.tv_sec ^ tv.tv_usec) % ip_count;
    for (i = 0; i < ip_count; ++i) {
      const ipv4addr* addr = &ips[(i + offset) % ip_count];
      if (lseek(fd, 0, SEEK_SET) != 0)
	return &resp_internal;
      if ((sock = try_connect(addr, cmdport, connect_timeout)) < 0)
	continue;

      if (ibuf_init(&netin, sock, 0, IOBUF_NEEDSCLOSE, 0)) {
	netin.io.timeout = timeout;
	if (write(sock, "STREAM\n", 7) == 7
	    && ibuf_getstr(&netin, &line, LF)
	    && memcmp(line.s, "PORT ", 5) == 0
	    && (dataport = strtoul(line.s+5, 0, 10)) > 0) {
	  if ((sock = try_connect(addr, dataport, connect_timeout)) >= 0) {
	    if (obuf_init(&netout, sock, 0, IOBUF_NEEDSCLOSE, 0)) {
	      netout.io.timeout = send_timeout;
	      if (obuf_copyfromfd(fd, &netout)
		  && obuf_close(&netout)
		  && ibuf_getstr(&netin, &line, LF)) {
		ibuf_close(&netin);
		if (memcmp(line.s, "stream: ", 8) == 0) {
		  str_lcut(&line, 8);
		  str_rstrip(&line);
		  if (str_diffs(&line, "OK") == 0)
		    return 0;
		  str_splices(&line, 0, 0, "5.7.0 Virus scan failed: ");
		  resp_virus.message = line.s;
		  return &resp_virus;
		}
	      }
	      obuf_close(&netout);
	    }
	    if (sock >= 0)
	      close(sock);
	  }
	}
	ibuf_close(&netin);
      }
      if (sock >= 0)
	close(sock);
    }
  }
  return &resp_no_scan;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .flags = FLAG_NEED_FILE,
  .message_end = message_end,
};
