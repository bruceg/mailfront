sfqmail() { smtpfront-qmail 2>/dev/null | tail +2; }
sfecho() { smtpfront-echo 2>/dev/null | tail +2; }
pfauth() { pop3front-auth "$@" echo Yes. 2>/dev/null | tail +2; }
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
