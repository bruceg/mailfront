#include <string.h>
#include "constants.h"
#include "sasl-auth.h"
#include "base64/base64.h"
#include "cvm-sasl/cvm-sasl.h"
#include "iobuf/iobuf.h"
#include "str/str.h"

static str challenge;
static str challenge64;
static str response;
static str response64;
static str mechanism;

int sasl_auth_init(void)
{
  return sasl_init();
}

int sasl_auth_cap(str* line)
{
  const sasl_mechanism* smech;
  if (!sasl_mechanisms) return 0;

  if (!str_truncate(line, 0) ||
      !str_copys(line, "AUTH")) return -1;
  for (smech = sasl_mechanisms; smech != 0; smech = smech->next)
    if (!str_catc(line, ' ') ||	!str_cats(line, smech->name)) return -1;
  return 1;
}

int sasl_auth(const char* prefix, const str* arg)
{
  unsigned s;
  int i;

  if ((s = str_findfirst(arg, SPACE)) != (unsigned)-1) {
    if (!str_copyb(&mechanism, arg->s, s)) return -1;
    if (!str_truncate(&response, 0)) return -1;
    while (arg->s[s] == SPACE) ++s;
    if (!base64_decode_line(arg->s+s, &response))
      return SASL_RESP_BAD;
    i = sasl_start(mechanism.s, &response, &challenge);
  }
  else
    i = sasl_start(arg->s, 0, &challenge);

  while (i == SASL_CHALLENGE) {
    if (!str_truncate(&challenge64, 0) ||
	!base64_encode_line(challenge.s, challenge.len, &challenge64))
      return -1;
    if (!obuf_puts(&outbuf, prefix) ||
	!obuf_write(&outbuf, challenge64.s, challenge64.len) ||
	!obuf_putsflush(&outbuf, CRLF))
      return -1;
    if (!ibuf_getstr_crlf(&inbuf, &response64)) return -1;
    if (response64.len == 0 || response64.s[0] == '*') return SASL_AUTH_FAILED;
    if (!str_truncate(&response, 0) ||
	!base64_decode_line(response64.s, &response))
      return SASL_RESP_BAD;
    i = sasl_response(&response, &challenge);
  }
  return i;
}

const char* sasl_auth_msg(int* code) 
{
  int newcode;
  const char* msg;
  #define R(C,M) newcode=C; msg=M; break
  switch (*code) {
  case SASL_AUTH_FAILED: R(501,"Authentication failed.");
  case SASL_NO_MECH: R(504,"Unrecognized authentication mechanism.");
  case SASL_RESP_REQUIRED: R(535,"Response was required but not given.");
  case SASL_RESP_NOTALLOWED: R(535,"Initial response not allowed.");
  case SASL_RESP_BAD: R(501,"Could not decode the response.");
  default: R(451,"Internal error.");
  }
  *code = newcode;
  return msg;
}
