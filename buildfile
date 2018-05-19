# file      : buildfile
# copyright : Copyright (c) 2009-2017 Code Synthesis Tools CC
# license   : GNU GPL v3; see accompanying LICENSE file

./: {*/ -build/ -m4/} doc{GPLv3 INSTALL LICENSE NEWS README} manifest

# Don't install the INSTALL file.
#
doc{INSTALL}@./: install = false
