#include <stdlib.h>
#include <unistd.h>
#include <bglibs/iobuf.h>
#include <bglibs/msg.h>
#include <bglibs/str.h>

#include <errno.h>
#include <gnutls/gnutls.h>

#include "mailfront.h"
#include "starttls.h"

#define DH_BITS 1024

static gnutls_certificate_credentials_t x509_cred;
static gnutls_priority_t priority_cache;
static gnutls_session_t gsession;

static int tls_available = 0;

static ibuf realinbuf;			/* underlying input stream */
static obuf realoutbuf;			/* underlying output stream */

static const char *certfile;
static const char *keyfile;

/*
 * TLS read and write functions for bglib, to fake out the rest of
 * mailfront
 */

static int tlsread(int n, void *ptr, unsigned long len)
{
  ssize_t ret;

  for(;;) {
    ret = gnutls_record_recv(gsession, ptr, (size_t)len);
    if (ret >= 0) return ret;
    if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED) continue;
    if (ret == GNUTLS_E_PREMATURE_TERMINATION) return 0;
    msgf("{TLS error }d", ret);
    return 0;
  }
  (void)n;
}

/* write in as many chunks as needed */
static int tlswrite(int n, void *ptr, unsigned long len)
{
  size_t ret;
  size_t tret = 0;

  for(;;) {
    ret = gnutls_record_send(gsession, ptr, (size_t)len);
    if (ret <= 0)
      return ret;
    tret += ret;
    if (ret >= len)
      return tret;
    len -= ret;
    ptr = (char *)ptr + ret;
  }
  (void)n;
}

/*
 * low-level read and write functions for gnutls
 * read returns at least one char, more if they're in the buffer
 */
static ssize_t llread(gnutls_transport_ptr_t p, void* buf, size_t size)
{
  int n;
  size_t r = 1;

  n = ibuf_getc(&realinbuf, buf++);
  if (!n) return 0;
  while (r < size && realinbuf.io.bufstart < realinbuf.io.buflen) {
    n = ibuf_getc(&realinbuf, buf++);
    if (!n) return 0;
    r++;
  }
  return r;
  (void)p;
}

static ssize_t llwrite(gnutls_transport_ptr_t p, void* buf, size_t size)
{
  int n;

  n = obuf_write(&realoutbuf, buf, size);
  obuf_flush(&realoutbuf);
  if (n) return size;
  return realoutbuf.count;	/* actual amount written */
  (void)p;
}

const response* starttls_init(void)
{
  certfile = getenv("TLS_CERTFILE");
  keyfile = getenv("TLS_KEYFILE");
  if (keyfile == NULL)
    keyfile = certfile;
  
  if (certfile && *certfile && keyfile && *keyfile) {
    tls_available = 1;
    if (getenv("TLS_IMMEDIATE")) {
      if (!starttls_start())
        exit(1);		/* not much else to do */
      return 0;
    }
  }

  return 0;
}

static gnutls_session_t initialize_tls_session(void)
{
  gnutls_session_t tlssession;

  gnutls_init(&tlssession, GNUTLS_SERVER);

  gnutls_priority_set(tlssession, priority_cache);

  gnutls_credentials_set(tlssession, GNUTLS_CRD_CERTIFICATE, x509_cred);

  /* request client certificate if any. */
  gnutls_certificate_server_set_request(tlssession, GNUTLS_CERT_REQUEST);

  /* Set maximum compatibility mode. */
  gnutls_session_enable_compatibility_mode(tlssession);

  return tlssession;
}

static gnutls_dh_params_t dh_params;

static str tlsparams;

static int didstarttls = 0;

int starttls_start(void)
{
  int ret;
  const char *p, *p2, *p3;
  const char *my_priority = getenv("TLS_PRIORITY");

  /* STARTTLS must be the last command in a pipeline, but too bad.
   * I don't think CVE-2011-0411 applies since the TLS handshake
   * consumes whtatever follows the STARTTLS command  */

  if (didstarttls) {
    msg2("already called", "gnutls global init");
    return 0;
  }
  didstarttls = 1;
  
  gnutls_global_init();

  gnutls_certificate_allocate_credentials(&x509_cred);
  ret = gnutls_certificate_set_x509_key_file(x509_cred, certfile, keyfile, GNUTLS_X509_FMT_PEM);
  if (ret != GNUTLS_E_SUCCESS) {
    msg2("TLS init failed ", gnutls_strerror(ret));
    return 0;
  }

  /* Generate Diffie-Hellman parameters - for use with DHE
   * kx algorithms. When short bit length is used, it might
   * be wise to regenerate parameters. */
  gnutls_dh_params_init(&dh_params);
  gnutls_dh_params_generate2(dh_params, DH_BITS);

  if (!my_priority)
    my_priority = "NORMAL";
  ret = gnutls_priority_init(&priority_cache, my_priority, NULL);
  if (ret != GNUTLS_E_SUCCESS) {
    msg2("TLS priority error ", gnutls_strerror(ret));
    return 0;
  }

  gnutls_certificate_set_dh_params(x509_cred, dh_params);

  gsession = initialize_tls_session();

  /* save input and output to feed into TLS engine via llread and llwrite */
  realinbuf = inbuf;
  realoutbuf = outbuf;

  /* Re-initialize input and output to use our local TLS-ized I/O */
  ibuf_init(&inbuf, -1, (ibuf_fn)*tlsread, 0, 0);
  obuf_init(&outbuf,-1, (obuf_fn)*tlswrite, 0, 0);

  gnutls_transport_set_pull_function(gsession, (gnutls_pull_func)llread);
  gnutls_transport_set_push_function(gsession, (gnutls_push_func)llwrite);

  msg1("Starting TLS handshake ");

  ret = gnutls_handshake(gsession);
  if (ret < 0) {
    gnutls_deinit(gsession);
    msg2("TLS handshake failed ", gnutls_strerror(ret));
    return 0;
  }
  p = gnutls_protocol_get_name(gnutls_protocol_get_version(gsession));
  p2 = gnutls_certificate_type_get_name(gnutls_certificate_type_get(gsession));
  p3 = gnutls_mac_get_name(gnutls_mac_get(gsession));
  str_copy5s(&tlsparams, p, "/" , p2, "/", p3);
  msg2("TLS handshake: ", tlsparams.s);
  session_setstr("tlsparams", tlsparams.s);
  return 1;
}

int starttls_available(void)
{
  return tls_available;
}

void starttls_disable(void)
{
  tls_available = 0;
}
