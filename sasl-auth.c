#include <string.h>
#include "constants.h"
#include "sasl-auth.h"
#include <base64/base64.h>
#include <cvm/sasl.h>
#include <cvm/client.h>
#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/str.h>

int sasl_auth_init(void)
{
  return sasl_init();
}

int sasl_auth_cap(str* caps)
{
  const sasl_mechanism* smech;
  if (!sasl_mechanisms) return 0;

  if (!str_truncate(caps, 0) ||
      !str_copys(caps, "AUTH")) return -1;
  for (smech = sasl_mechanisms; smech != 0; smech = smech->next)
    if (!str_catc(caps, ' ') ||	!str_cats(caps, smech->name)) return -1;
  return 1;
}

int sasl_auth2(const char* prefix,
	       const char* mechanism, const char* iresponse)
{
  static str challenge;
  static str challenge64;
  static str response;
  static str response64;
  int i;
  str* iresponsestr;

  if (iresponse != 0) {
    if (!str_truncate(&response, 0)) return -1;
    if (!base64_decode_line(iresponse, &response)) {
      msg3("SASL AUTH ", mechanism, " failed: bad response");
      return SASL_RESP_BAD;
    }
    iresponsestr = &response;
  }
  else
    iresponsestr = 0;
  for (i = sasl_start(mechanism, iresponsestr, &challenge);
       i == SASL_CHALLENGE;
       i = sasl_response(&response, &challenge)) {
    if (!str_truncate(&challenge64, 0) ||
	!base64_encode_line(challenge.s, challenge.len, &challenge64))
      return -1;
    if (!obuf_puts(&outbuf, prefix) ||
	!obuf_write(&outbuf, challenge64.s, challenge64.len) ||
	!obuf_putsflush(&outbuf, CRLF))
      return -1;
    if (!ibuf_getstr_crlf(&inbuf, &response64)) return -1;
    if (response64.len == 0 || response64.s[0] == '*') {
      msg3("SASL AUTH ", mechanism, " failed: aborted");
      return SASL_AUTH_FAILED;
    }
    if (!str_truncate(&response, 0) ||
	!base64_decode_line(response64.s, &response)) {
      msg3("SASL AUTH ", mechanism, " failed: bad response");
      return SASL_RESP_BAD;
    }
  }
  if (i == SASL_AUTH_OK) {
    str_truncate(&response, 0);
    str_copys(&response, "username=");
    str_cats(&response, cvm_fact_username);
    if (cvm_fact_sys_username != 0) {
      str_cats(&response, " sys_username=");
      str_cats(&response, cvm_fact_sys_username);
    }
    if (cvm_fact_domain != 0 && cvm_fact_domain[0] != 0) {
      str_cats(&response, " domain=");
      str_cats(&response, cvm_fact_domain);
    }
    msg4("SASL AUTH ", mechanism, " ", response.s);
  }
  else
    msg3("SASL AUTH ", mechanism, " failed");
  return i;
}

int sasl_auth1(const char* prefix, const str* arg)
{
  static str mechanism;
  int s;
  if ((s = str_findfirst(arg, SPACE)) != -1) {
    if (!str_copyb(&mechanism, arg->s, s)) return -1;
    while (arg->s[s] == SPACE) ++s;
    return sasl_auth2(prefix, mechanism.s, arg->s+s);
  }
  else
    return sasl_auth2(prefix, arg->s, 0);
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
