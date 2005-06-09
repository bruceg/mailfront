#include <stdlib.h>
#include <string.h>

#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <msg/wrap.h>
#include <str/str.h>

#include "mailfront.h"
#include "mailrules.h"

struct pattern
{
  int mode;
  str s;
  const char* message;
};

#define T_AFTER_BLANK ('\\')
#define T_RESPONSE ('=')
#define T_COMMENT ('#')
#define T_HEADER (':')
#define T_NORMAL (0)

static response resp_patmatch = { 554, 0 };
static struct pattern* patterns;
static str responses;
static unsigned pattern_count;
static int linemode;
static unsigned linepos;
static unsigned linemax = 256;
static char* linebuf;

static int patterns_read(const char* filename)
{
  ibuf in;
  int mode;
  unsigned i;
  unsigned count;
  str line = {0,0,0};
  const char* currmsg = "This message contains prohibited content";
  if (!ibuf_open(&in, filename, 0)) return 0;
  count = 0;
  /* Count the number of non-comment lines. */
  while (ibuf_getstr(&in, &line, LF)) {
    str_rstrip(&line);
    if (line.len > 0 && line.s[0] != T_COMMENT) {
      if (line.s[0] == T_RESPONSE)
	/* Pre-allocate room for the responses */
	wrap_str(str_catb(&responses, line.s+1, line.len));
      else
	++count;
    }
  }
  responses.len = 0;
  /* Allocate the lines. */
  if ((patterns = malloc(count * sizeof *patterns)) == 0)
    die_oom(111);
  if (!ibuf_rewind(&in))
    die1sys(111, "Could not rewind patterns file");
  memset(patterns, 0, count * sizeof *patterns);
  for (i = 0; i < count && ibuf_getstr(&in, &line, LF); ) {
    str_rstrip(&line);
    if (line.len > 0) {
      switch (mode = line.s[0]) {
      case T_COMMENT:
	continue;
      case T_RESPONSE:
	currmsg = responses.s + responses.len;
	str_catb(&responses, line.s+1, line.len);
	continue;
      case T_AFTER_BLANK:
      case T_HEADER:
	break;
      default:
	mode = T_NORMAL;
      }
      patterns[i].mode = mode;
      wrap_str(str_copyb(&patterns[i].s, line.s+1, line.len-1));
      patterns[i].message = currmsg;
      ++i;
    }
  }
  pattern_count = i;
  ibuf_close(&in);
  str_free(&line);
  return 1;
}

void patterns_init(void)
{
  const char* tmp;
  unsigned u;
  if ((tmp = rules_getenv("PATTERNS")) != 0)
    if (!patterns_read(tmp))
      warn3sys("Could not read patterns file '", tmp, "'");
  if ((tmp = rules_getenv("PATTERNS_LINEMAX")) != 0)
    if ((u = strtoul(tmp, (char**)&tmp, 10)) > 0 && *tmp == 0)
      linemax = u;
  if ((linebuf = malloc(linemax+1)) == 0)
    return;
  linemode = T_HEADER;
  linepos = 0;
}

static const response* check_line()
{
  unsigned i;
  struct pattern* p;
  const str fakeline = { linebuf, linepos, 0 };
  linebuf[linepos] = 0;
  for (p = patterns, i = 0; i < pattern_count; ++i, ++p) {
    if ((p->mode == T_NORMAL
	 || p->mode == linemode)
	&& str_glob(&fakeline, &p->s)) {
      resp_patmatch.message = p->message;
      return &resp_patmatch;
    }
  }
  return 0;
}

const response* patterns_check(const char* bytes, unsigned len)
{
  const char* p;
  const char* const end = bytes + len;
  const response* r;
  if (linebuf == 0) return 0;
  for (p = bytes; p < end; ++p) {
    const char ch = *p;
    if (ch == LF) {
      if (linepos > 0) {
	if ((r = check_line()) != 0)
	  return r;
	if (linemode != T_HEADER)
	  linemode = T_NORMAL;
      }
      else
	linemode = T_AFTER_BLANK;
      linepos = 0;
    }
    else {
      if (linepos < linemax)
	linebuf[linepos++] = ch;
    }
  }
  return 0;
}
