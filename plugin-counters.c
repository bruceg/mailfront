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
  session.maxdatabytes = session_getenvu("DATABYTES");
  return 0;
}

static const response* reset(void)
{
  rcpt_count = 0;
  return 0;
}

static void minenv(unsigned long* u, const char* name)
{
  unsigned long value = session_getenvu(name);
  if (value > 0
      && (*u == 0
	  || *u > value))
    *u = value;
}

static const response* sender(str* r)
{
  /* This MUST be done as a sender match to make sure SMTP "MAIL FROM"
   * commands with a SIZE parameter can be rejected properly. */
  minenv(&session.maxdatabytes, "DATABYTES");
  minenv(&session.maxrcpts, "MAXRCPTS");
  (void)r;
  return 0;
}

static const response* recipient(str* r)
{
  minenv(&session.maxdatabytes, "DATABYTES");
  minenv(&session.maxrcpts, "MAXRCPTS");
  ++rcpt_count;
  if (session.maxrcpts > 0 && rcpt_count > session.maxrcpts)
    return &resp_manyrcpt;
  return 0;
  (void)r;
}

static const response* start(void)
{
  minenv(&session.maxdatabytes, "DATABYTES");
  if ((session.maxhops = session_getenvu("MAXHOPS")) == 0)
    session.maxhops = 100;
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
  data_bytes += len;
  if (session.maxdatabytes && data_bytes > session.maxdatabytes)
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
	    if (++count_rec > session.maxhops)
	      return &resp_hops;
	  }
	}
	if (in_dt) {
	  if (ch != "delivered-to:"[linepos] &&
	      ch != "DELIVERED-TO:"[linepos])
	    in_dt = 0;
	  else if (linepos >= 12) {
	    in_dt = in_rec = 0;
	    if (++count_dt > session.maxhops)
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
