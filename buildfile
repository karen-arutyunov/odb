# file      : buildfile
# license   : GNU GPL v3; see accompanying LICENSE file

./: {*/ -build/ -m4/} doc{GPLv3 INSTALL LICENSE NEWS README} manifest

# Don't install tests or the INSTALL file.
#
tests/:          install = false
doc{INSTALL}@./: install = false
