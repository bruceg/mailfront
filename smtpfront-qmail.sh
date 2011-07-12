exec "$(dirname $0)"/mailfront smtp qmail cvm-authenticate check-fqdn counters mailrules relayclient cvm-validate qmail-validate add-received patterns "$@"
