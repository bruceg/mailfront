# Template makedist.py file

# Set WEBSITE to the name of the web site that this package will be
# deposited in.  The URL will always be:
#	http://$WEBSITE/$PACKAGE/
WEBSITE = 'untroubled.org'

# If LISTSUB is set, makedist will add a note regarding mailing list
# subscription.
LISTSUB = 'bgware-subscribe@lists.untroubled.org'
LISTURL = 'http://lists.untroubled.org/?list=bgware'

# Set EXTRAS to a list of any extra files that should go into the
# base directory in the destination site.
EXTRAS = [
	'ANNOUNCEMENT',
	'ChangeLog',
	'NEWS',
	'README',
	'TODO',
	'*.html',
	'plugin-template.c',
	]

# Set RPMUPLOAD to a list of additional hostname/path destinations to
# which to upload the source and binary RPMs.
RPMUPLOAD = [
	#( 'incoming.redhat.com', 'libc6' ),
	]

# Set LISTS to the mailing list(s) to send the announcement to
LISTS = [
	'bgware@lists.untroubled.org',
	]

# Run any extra commands to prepare the source tree (such as making
# documentation) here.
