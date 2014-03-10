Entrelacs system first prototype.
See http://code.google.com/p/entrelacs/

Code released under GPLv3 http://www.gnu.org/licenses/gpl.html

Credits:
_______
   * sha1.c / sha1.h : immoderate copy of PolarSSL sha1 functions. http://www.polarssl.org/
   * log.c / log.h : shameless minidlna source copy
   * other bites of codes from here and there
   * mongoose HTTP server : http://code.google.com/p/mongoose/

Install & test:
______________
make all ; # compile everythings
make libentrelacs.a libentrelacs.so ; # compile libraries
make tests ; # also compile tests
make server ; # compile the server
make start ; # compile then start the HTTP server
make run ; # compile and perform regression tests
make help ; # echo additional suggestions
make clean ; # clean in order to rebuild
make clean.sometest : # clean test exe

Additional configuration:
________________________
- See options in servers.c

Notable files :
________________
libentrelacs.a ; # static library for the entrelacs "core"
libentrelacs.so ; # dynamic library
entrelacsd ; # entrelacs HTTP server
cli.sh ; # simple script to query the server (depends on curl cmd-line client)
testshell ; # simple shell
site.xl ; # collection of URL to set up a demo site on top of an HTTP server

