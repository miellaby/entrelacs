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
make tests ; # compile and tests everythings (type Ctrl+D when in the shell)
make help ; # see other build commands

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

