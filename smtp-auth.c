#include "mailfront.h"
#include "smtp.h"
#include "base64/base64.h"
#include "cvm-sasl/cvm-sasl.h"

int smtp_auth_init(void)
{
  return sasl_init();
}

int smtp_auth_cap(str* line)
{
  sasl_mechanism* smech;
  if (!str_truncate(line, 0) ||
      !str_copys(line, "AUTH")) return -1;
  for (smech = sasl_mechanisms; smech->name != 0; smech++)
    if (smech->cvm)
      if (!str_catc(line, ' ') ||
	  !str_cats(line, smech->name))
	return -1;
  return line->len > 4;
}

static RESPONSE(internal, 451, "Internal error.");
static RESPONSE(auth_already, 503, "You are already authenticated.");
static RESPONSE(auth_failed, 501, "Authentication failed.");
static RESPONSE(auth_badresp, 501, "Could not decode the response.");
static RESPONSE(auth_respnotallowed, 535, "Initial response not allowed.");
static RESPONSE(auth_nomech, 504, "Unrecognized authentication type.");
static RESPONSE(auth_respreq, 535, "Response was required but not given.");
static RESPONSE(auth_ok, 235, "Authentication succeeded.");

int smtp_auth(str* arg)
{
  static str challenge;
  static str challenge64;
  static str response;
  unsigned s;
  int i;

  if ((s = str_findfirst(arg, SPACE)) != (unsigned)-1) {
    arg->s[s++] = 0;
    if (!str_truncate(&response, 0) ||
	!base64_decode_line(arg->s+s, &response))
      return respond_resp(&resp_auth_badresp, 1);
    i = sasl_start(arg->s, &response, &challenge);
  }
  else
    i = sasl_start(arg->s, 0, &challenge);
  
  while (i != SASL_AUTH_OK) {
    switch (i) {
    case SASL_AUTH_FAILED: return respond_resp(&resp_auth_failed, 1);
    case SASL_NO_MECH: return respond_resp(&resp_auth_nomech, 1);
    case SASL_RESP_REQUIRED: return respond_resp(&resp_auth_respreq, 1);
    case SASL_RESP_NOTALLOWED: return respond_resp(&resp_auth_respnotallowed, 1);
    case SASL_RESP_BAD: return respond_resp(&resp_auth_badresp, 1);
    case SASL_CHALLENGE:
      if (!str_truncate(&challenge64, 0) ||
	  !base64_encode_line(challenge.s, challenge.len, &challenge64))
	return respond_resp(&resp_internal, 1);
      if (!respond(334, 1, challenge64.s)) return 0;
      if (!smtp_get_line()) return 0;
      if (!str_truncate(&response, 0) ||
	  !base64_decode_line(line.s, &response))
	return respond_resp(&resp_auth_badresp, 1);
      i = sasl_response(&response, &challenge);
      break;
    default: return respond_resp(&resp_internal, 1);
    }
  }
  authenticated = 1;
  return respond_resp(&resp_auth_ok, 1);
}
