sfqmail() { smtpfront-qmail 2>/dev/null | tail +2; }
sfecho() { smtpfront-echo 2>/dev/null | tail +2; }
pfauth() { pop3front-auth "$@" echo Yes. 2>/dev/null | tail +2; }
ifauth() { imapfront-auth sh -c 'echo Yes: $IMAPLOGINTAG' 2>/dev/null | tail +2; }
pfmaildir() { pop3front-maildir "$@" 2>/dev/null | tail +2; }
maildir=$tmp/Maildir
maildir() {
  rm -rf $maildir
  mkdir -p $maildir/{cur,new,tmp}
}
tstmsg() {
  fn=$1
  {
    echo "Header: foo"
    echo
    echo "body"
  } >$maildir/$fn
}

PROTO=TEST
TESTLOCALIP=1.2.3.4
TESTREMOTEIP=5.6.7.8
TESTLOCALHOST=local.host
TESTREMOTEHOST=remote.host

export PROTO TESTLOCALIP TESTREMOTEIP TESTLOCALHOST TESTREMOTEHOST

cat >$tmp/cvm <<EOF
#!/bin/sh
req="\`cat -v\`"
if [ "x\$req" = 'x^Atestuser^@^@testpass^@^@' ]; then
  printf '\\0\\0010500\\0\\1testuser\\0\\0002500\\0\\0003500\\0\\4Test User\\0\\5$tmp\\0\\6/bin/false\\0\\7testgroup\\0\\0'
  exit 0
fi
printf \\\\0144
exit 100
EOF
chmod +x $tmp/cvm
