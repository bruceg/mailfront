#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mailfront.h"

#include <cdb/cdb.h>
#include <dict/dict.h>
#include <dict/load.h>
#include <iobuf/iobuf.h>
#include <str/str.h>

static RESPONSE(erropen,421,"4.3.0 Could not open $MAILRULES file");
static RESPONSE(syntax,421,"4.3.0 Syntax error in $MAILRULES");
static RESPONSE(erropenref,421,"4.3.0 Error opening file referenced from $MAILRULES");

#define NUL 0

/* rule tables ************************************************************* */
struct pattern
{
  str pattern;
  dict* file;
  struct cdb* cdb;
  int negated;
};

struct rule
{
  int code;
  struct pattern sender;
  struct pattern recipient;
  str response;
  str relayclient;
  str environment;
  unsigned long databytes;
  struct rule* next;
};

static struct rule* sender_rules = 0;
static struct rule* recip_rules = 0;
static struct rule** current_rules = 0;
static void append_rule(struct rule* r)
{
  static struct rule* sender_tail = 0;
  static struct rule* recip_tail = 0;
  struct rule** head;
  struct rule** tail;
  if (current_rules != 0)
    tail = ((head = current_rules) == &sender_rules) ?
      &sender_tail : &recip_tail;
  else if (r->recipient.pattern.len == 1 && r->recipient.pattern.s[0] == '*') {
    head = &sender_rules;
    tail = &sender_tail;
  }
  else {
    head = &recip_rules;
    tail = &recip_tail;
  }
  if (*tail == 0)
    *head = r;
  else
    (*tail)->next = r;
  *tail = r;
}

static struct rule* alloc_rule(void)
{
  struct rule* r;
  if ((r = malloc(sizeof *r)) != 0)
    memset(r, 0, sizeof *r);
  return r;
}

#if 0
static void free_rule(struct rule* r)
{
  str_free(&r->sender.pattern);
  str_free(&r->recipient.pattern);
  str_free(&r->response);
  str_free(&r->relayclient);
  str_free(&r->environment);
  free(r);
}
#endif

/* embeded filename handling *********************************************** */
static dict cdb_files;
static struct cdb* open_cdb(const str* filename)
{
  int fd;
  struct cdb* c;
  if ((c = malloc(sizeof *c)) == 0) return 0;
  fd = open(filename->s, O_RDONLY);
  cdb_init(c, fd);
  if (!dict_add(&cdb_files, filename, c)) return 0;
  return c;
}

static int lower(str* s)
{
  str_lower(s);
  return 1;
}

static dict text_files;
static dict* load_text(const str* filename)
{
  dict* d;
  if ((d = malloc(sizeof *d)) == 0) return 0;
  memset(d, 0, sizeof *d);
  if (!dict_load_list(d, filename->s, 1, lower)) return 0;
  if (!dict_add(&text_files, filename, d)) return 0;
  return d;
}

static int is_special(const str* pattern)
{
  long len = pattern->len;
  return len > 5 &&
    pattern->s[0] == '[' &&
    pattern->s[1] == '[' &&
    pattern->s[len-2] == ']' &&
    pattern->s[len-1] == ']';
}

static int is_cdb(const str* filename)
{
  const char* end = filename->s + filename->len;
  return filename->len > 4 &&
    end[-4] == '.' &&
    end[-3] == 'c' &&
    end[-2] == 'd' &&
    end[-1] == 'b';
}

static str filename;
static int extract_filename(const str* pattern)
{
  if (!is_special(pattern)) return 0;
  if (pattern->s[2] == '@')
    str_copyb(&filename, pattern->s+3, pattern->len-5);
  else
    str_copyb(&filename, pattern->s+2, pattern->len-4);
  return 1;
}

static int try_load(struct pattern* pattern)
{
  if (extract_filename(&pattern->pattern)) {
    if (is_cdb(&filename))
      return (pattern->cdb = open_cdb(&filename)) != 0;
    else 
      return (pattern->file = load_text(&filename)) != 0;
  }
  return 1;
}

static void apply_environment(const str* s)
{
  unsigned i;
  unsigned len;
  for (i = 0; i < s->len; i += len + 1) {
    len = strlen(s->s + i);
    if (session.env.len > 0)
      str_catc(&session.env, NUL);
    str_catb(&session.env, s->s + i, len);
  }
}

/* file parsing ************************************************************ */
static int isoctal(int ch) { return ch >= '0' && ch <= '8'; }

static const char* parse_uint(const char* ptr, char sep,
			      unsigned long* out)
{
  for (*out = 0; *ptr != 0 && *ptr != sep; ++ptr) {
    if (*ptr >= '0' && *ptr <= '9')
      *out = (*out * 10) + (*ptr - '0');
    else
      return 0;
  }
  return ptr;
}

static const char* parse_char(const char* ptr, char* out)
{
  int o;
  switch (*ptr) {
  case 0: return ptr;
  case '\\':
    switch (*++ptr) {
    case 'n': *out = '\n'; break;
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7':
      o = *ptr - '0';
      if (isoctal(ptr[1])) {
	o = (o << 3) | (*++ptr - '0');
	if (isoctal(ptr[1]))
	  o = (o << 3) | (*++ptr - '0');
      }
      *out = 0;
      break;
    default: *out = *ptr;
    }
    break;
  default: *out = *ptr;
  }
  return ptr + 1;
}

static const char* parse_str(const char* ptr, char sep, str* out)
{
  char ch = 0;
  /* str_truncate(out, 0); */
  for (;;) {
    if (*ptr == sep || *ptr == NUL) return ptr;
    ptr = parse_char(ptr, &ch);
    str_catc(out, ch);
  }
  return ptr;
}

static const char* parse_pattern(const char* ptr, char sep,
				 struct pattern* out)
{
  while (*ptr != sep && *ptr == '!') {
    out->negated = !out->negated;
    ++ptr;
  }
  return parse_str(ptr, sep, &out->pattern);
}

static void parse_env(const char* ptr, str* out)
{
  while (ptr && *ptr != 0)
    if ((ptr = parse_str(ptr, ',', out)) != 0) {
      str_catc(out, 0);
      if (*ptr == ',') ++ptr;
    }
}

static const response* add(const char* l)
{
  struct rule* r;
  
  if (*l != 'k' && *l != 'd' && *l != 'z' && *l != 'p' && *l != 'n') return 0;
  r = alloc_rule();
  r->code = *l++;

  if ((l = parse_pattern(l, ':', &r->sender)) != 0 && *l == ':')
    if ((l = parse_pattern(l+1, ':', &r->recipient)) != 0 && *l == ':')
      if ((l = parse_str(l+1, ':', &r->response)) != 0 && *l == ':')
	if ((l = parse_uint(l+1, ':', &r->databytes)) != 0)
	  if (*l == ':'
	      && (l = parse_str(l+1, ':', &r->relayclient)) != 0
	      && *l == ':')
	    parse_env(l+1, &r->environment);

  if (l == 0) return &resp_syntax;
  append_rule(r);

  /* Pre-load text files and pre-open CDB files */
  if (!try_load(&r->sender)) return &resp_erropenref;
  if (!try_load(&r->recipient)) return &resp_erropenref;
  return 0;
}

static int loaded = 0;

static const response* init(void)
{
  const char* path;
  str rule = {0,0,0};
  ibuf in;
  const response* r;
  
  if ((path = getenv("MAILRULES")) == 0) return 0;
  loaded = 1;

  if (!ibuf_open(&in, path, 0)) return &resp_erropen;
  while (ibuf_getstr(&in, &rule, LF)) {
    str_strip(&rule);
    if (rule.len == 0) continue;
    if (rule.s[0] == ':') {
      switch (rule.s[1]) {
      case 's': current_rules = &sender_rules; break;
      case 'r': current_rules = &recip_rules; break;
      default: return &resp_syntax;
      }
    }
    else if ((r = add(rule.s)) != 0)
      return r;
  }
  ibuf_close(&in);
  str_free(&rule);
  return 0;
}

static const response* reset(void)
{
  if (loaded)
    session_resetenv();
  return 0;
}

/* rule application ******************************************************** */
static int matches(const struct pattern* pattern,
		   const str* addr, const str* atdomain)
{
  static str domain;
  int result;
  if (pattern->cdb != 0) {
    if (pattern->pattern.s[2] == '@')
      result = cdb_find(pattern->cdb, atdomain->s+1, atdomain->len-1) != 0;
    else {
      result = (cdb_find(pattern->cdb, addr->s, addr->len) != 0) ?
	1 :
	cdb_find(pattern->cdb, atdomain->s, atdomain->len) != 0;
    }
  }
  else if (pattern->file != 0) {
    if (pattern->pattern.s[2] == '@') {
      str_copyb(&domain, atdomain->s+1, atdomain->len-1);
      result = dict_get(pattern->file, &domain) != 0;
    }
    else {
      result = (dict_get(pattern->file, addr) != 0) ?
	1 : 
	dict_get(pattern->file, atdomain) != 0;
    }
  }
  else
    result = str_case_glob(addr, &pattern->pattern);
  if (pattern->negated)
    result = !result;
  return result;
}

static const response* build_response(int type, const str* message)
{
  static response resp;
  unsigned code;
  const char* defmsg;

  switch (type) {
  case 'p': return 0;
  case 'n': return 0;
  case 'k': code = 250; defmsg = "OK"; break;
  case 'd': code = 553; defmsg = "Rejected"; break;
  case 'z': code = 451; defmsg = "Deferred"; break;
  default:  code = 451; defmsg = "Temporary failure"; break;
  }

  resp.number = code;
  resp.message = (message->len == 0) ? defmsg : message->s;
  return &resp;
}

static const response* apply_rule(const struct rule* rule)
{
  const response* resp;
  resp = build_response(rule->code, &rule->response);
  apply_environment(&rule->environment);
  if (session.maxdatabytes == 0
      || (rule->databytes > 0
	  && session.maxdatabytes > rule->databytes))
    session.maxdatabytes = rule->databytes;
  return resp;
}

static void copy_addr(const str* addr,
		      str* saved, str* domain)
{
  int at;
  str_copy(saved, addr);
  str_lower(saved);
  if ((at = str_findlast(saved, '@')) != -1)
    str_copyb(domain, saved->s + at, saved->len - at);
  else
    str_truncate(domain, 0);
}

static str saved_sender;
static str sender_domain;

static const response* validate_sender(str* sender)
{
  struct rule* rule;
  const response* r;
  
  if (!loaded) return 0;
  copy_addr(sender, &saved_sender, &sender_domain);
  for (rule = sender_rules; rule != 0; rule = rule->next)
    if (matches(&rule->sender, &saved_sender, &sender_domain)) {
      r = apply_rule(rule);
      if (rule->code != 'n')
	return r;
    }
  return 0;
}

static str laddr;
static str rdomain;

static const response* validate_recipient(str* recipient)
{
  struct rule* rule;
  const response* r;
  
  if (!loaded) return 0;
  copy_addr(recipient, &laddr, &rdomain);
  for (rule = recip_rules; rule != 0; rule = rule->next)
    if (matches(&rule->sender, &saved_sender, &sender_domain) &&
	matches(&rule->recipient, &laddr, &rdomain)) {
      str_cat(recipient, &rule->relayclient);
      r = apply_rule(rule);
      if (rule->code != 'n')
	return r;
    }
  return 0;
}

struct module mailrules = {
  .init = init,
  .reset = reset,
  .sender = validate_sender,
  .recipient = validate_recipient,
};
