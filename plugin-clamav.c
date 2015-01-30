#include "mailfront.h"

#include <bglibs/sysdeps.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bglibs/msg.h>
#include <bglibs/resolve.h>
#include <bglibs/socket.h>
#include <bglibs/uint32.h>

static RESPONSE(no_hostname,451,"4.3.0 Could not resolve virus scanner hostname");
static RESPONSE(no_scan,451,"4.3.0 Could not virus scan message");
static response resp_virus = { 554, 0 };

#define MAX_IPS 16
#define MAX_CHUNK_SIZE 0xffffffffUL

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

static int copystream(int fd, size_t size, obuf* out)
{
  unsigned char prefix[4];
  uint32_pack_msb(size, prefix);
  if (!obuf_write(out, (char*)prefix, 4)) return 0;
  if (!obuf_copyfromfd(fd, out)) return 0;
  memset(prefix, 0, 4);
  return obuf_write(out, (char*)prefix, 4);
}

static const response* message_end(int fd)
{
  const char* hostname;
  const char* tmp;
  ipv4port cmdport;
  ipv4addr ips[MAX_IPS];
  int ip_count;
  int i;
  int offset;
  int result;
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

    if (fstat(fd, &st) != 0)
      return &resp_internal;
    /* For simplicity, this plugin just sends a single chunk, but each
     * chunk is limited to 2^32 bytes by the protocol. */
    if (st.st_size > 0xffffffffLL) {
      warn1("ClamAV scanning skipped: message larger than chunk size");
      return 0;
    }
    if ((tmp = session_getenv("CLAMAV_MAXSIZE")) != 0
	&& (maxsize = strtoul(tmp, (char**)&tmp, 10)) != 0
	&& *tmp == 0) {
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

      if (obuf_init(&netout, sock, 0, 0, 0)) {
	netout.io.timeout = send_timeout;
	result = obuf_puts(&netout, "nINSTREAM\n")
	  && copystream(fd, st.st_size, &netout)
	  && obuf_close(&netout);
	obuf_close(&netout);
	if (result) {
	  if (ibuf_init(&netin, sock, 0, IOBUF_NEEDSCLOSE, 0)) {
	    netin.io.timeout = timeout;
	    result = ibuf_getstr(&netin, &line, LF);
	    ibuf_close(&netin);
	    sock = -1;
	    if (result) {
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
	  }
	}
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
