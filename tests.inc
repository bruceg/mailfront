sfqmail() { smtpfront-qmail 2>/dev/null | tail +2; }
pfauth() { pop3front-auth "$@" echo Yes. | tail +2; }
