exec "$(dirname $0)"/mailfront qmqp qmail check-fqdn counters mailrules relayclient qmail-validate cvm-validate add-received patterns accept-sender "$@"
