#include <sysdeps.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <msg/msg.h>
#include "mailfront.h"

static response resp = { 250, 0 };
static str tmp;
static unsigned long databytes = 0;

static const response* reset(void)
{
  databytes = 0;
  return 0;
}

static const response* sender(str* s)
{
  str_copys(&tmp, "Sender='");
  str_cat(&tmp, s);
  str_cats(&tmp, "'.");
  resp.message = tmp.s;
  return &resp;
}

static const response* recipient(str* r)
{
  str_copys(&tmp, "Recipient='");
  str_cat(&tmp, r);
  str_cats(&tmp, "'.");
  resp.message = tmp.s;
  return &resp;
}

static const response* data_block(const char* bytes, unsigned long len)
{
  if (databytes == 0) {
    /* First line is always Received, log the first two lines. */
    const char* ch;
    ch = strchr(bytes, '\n');
    str_copyb(&tmp, bytes, ch-bytes);
    bytes = ch + 1;
    if ((ch = strchr(bytes, '\n')) != 0)
      str_catb(&tmp, bytes, ch-bytes);
    msg1(tmp.s);
  }
  databytes += len;
  return 0;
}

static const response* message_end(int fd)
{
  struct stat st;
  char buf[1024];
  long rd;
  char* lf;
  char* ptr;

  if (fd >= 0) {
    /* Log the first two lines of the message, usually a Received: header */
    lseek(fd, 0, SEEK_SET);
    rd = read(fd, buf, sizeof buf - 1);
    buf[rd] = 0;
    if ((lf = strchr(buf, LF)) != 0) {
      str_copyb(&tmp, buf, lf-buf);
      ptr = lf + 1;
      if ((lf = strchr(ptr, LF)) != 0)
	str_catb(&tmp, ptr, lf-ptr);
      msg1(tmp.s);
    }

    fstat(fd, &st);
    databytes = st.st_size;
  }

  str_copys(&tmp, "Received ");
  str_catu(&tmp, databytes);
  str_cats(&tmp, " bytes.");
  resp.message = tmp.s;
  return &resp;
  (void)fd;
}

struct plugin backend = {
  .version = PLUGIN_VERSION,
  .reset = reset,
  .sender = sender,
  .recipient = recipient,
  .data_block = data_block,
  .message_end = message_end,
};
