sfqmail() { smtpfront-qmail 2>/dev/null | tail +2; }
pfauth() { pop3front-auth "$@" echo Yes. | tail +2; }
pfmaildir() { pop3front-maildir "$@" | tail +2; }
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
