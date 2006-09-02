#include "mailfront.h"

static RESPONSE(too_long, 552, "5.2.3 Sorry, that message exceeds the maximum message length.");
static RESPONSE(hops, 554, "5.6.0 This message is looping, too many hops.");
static RESPONSE(manyrcpt, 550, "5.5.3 Too many recipients");

static unsigned rcpt_count = 0;
static unsigned long data_bytes; /* Total number of data bytes */
static unsigned count_rec;	/* Count of the Received: headers */
static unsigned count_dt;	/* Count of the Delivered-To: headers */
static int in_header;		/* True until a blank line is seen */
static unsigned linepos;     /* The number of bytes since the last LF */
static int in_rec;	      /* True if we might be seeing Received: */
static int in_dt;	  /* True if we might be seeing Delivered-To: */

static const response* init(void)
{
  /* This MUST be done in the init section to make sure the SMTP
   * greeting displays the current value. */
  session_setnum("maxdatabytes", session_getenvu("DATABYTES"));
  return 0;
}

static const response* reset(void)
{
  rcpt_count = 0;
  return 0;
}

static unsigned long minenv(const char* sname, const char* name)
{
  unsigned long u;
  unsigned long value = session_getenvu(name);
  u = 0;
  if (value > 0)
    if (!session_hasnum(sname, &u)
	|| u > value) {
      session_setnum(sname, value);
      return value;
    }
  return u;
}

static const response* sender(str* r)
{
  /* This MUST be done as a sender match to make sure SMTP "MAIL FROM"
   * commands with a SIZE parameter can be rejected properly. */
  minenv("maxdatabytes", "DATABYTES");
  minenv("maxrcpts", "MAXRCPTS");
  (void)r;
  return 0;
}

static const response* recipient(str* r)
{
  unsigned long maxrcpts = minenv("maxrcpts", "MAXRCPTS");
  minenv("maxdatabytes", "DATABYTES");
  ++rcpt_count;
  if (maxrcpts > 0 && rcpt_count > maxrcpts)
    return &resp_manyrcpt;
  return 0;
  (void)r;
}

static const response* start(void)
{
  unsigned long maxhops;
  minenv("maxdatabytes", "DATABYTES");
  if ((maxhops = session_getenvu("MAXHOPS")) == 0)
    maxhops = 100;
  session_setnum("maxhops", maxhops);
  data_bytes = 0;
  count_rec = 0;
  count_dt = 0;
  in_header = 1;
  linepos = 0;
  in_rec = 1;
  in_dt = 1;
  return 0;
}

static const response* block(const char* bytes, unsigned long len)
{
  unsigned i;
  const char* p;
  const unsigned long maxdatabytes = session_getnum("maxdatabytes", ~0UL);
  const unsigned long maxhops = session_getnum("maxhops", 100);
  data_bytes += len;
  if (maxdatabytes > 0 && data_bytes > maxdatabytes)
    return &resp_too_long;
  for (i = 0, p = bytes; in_header && i < len; ++i, ++p) {
    char ch = *p;
    if (ch == LF) {
      if (linepos == 0) in_header = 0;
      linepos = 0;
      in_rec = in_dt = in_header;
    }
    else {
      if (in_header && linepos < 13) {
	if (in_rec) {
	  if (ch != "received:"[linepos] &&
	      ch != "RECEIVED:"[linepos])
	    in_rec = 0;
	  else if (linepos >= 8) {
	    in_dt = in_rec = 0;
	    if (++count_rec > maxhops)
	      return &resp_hops;
	  }
	}
	if (in_dt) {
	  if (ch != "delivered-to:"[linepos] &&
	      ch != "DELIVERED-TO:"[linepos])
	    in_dt = 0;
	  else if (linepos >= 12) {
	    in_dt = in_rec = 0;
	    if (++count_dt > maxhops)
	      return &resp_hops;
	  }
	}
	++linepos;
      }
    }
  }
  return 0;
}

struct plugin plugin = {
  .init = init,
  .reset = reset,
  .sender = sender,
  .recipient = recipient,
  .data_start = start,
  .data_block = block,
};
