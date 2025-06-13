Entrelacs system first prototype.

Visit <https://miellaby.github.io/entrelacs/> for more information.

Code released under GPLv3 http://www.gnu.org/licenses/gpl.html

Credits:
_______
   * sha1.c / sha1.h : immoderate copy of PolarSSL sha1 functions. http://www.polarssl.org/
   * log.c / log.h : shameless minidlna source copy
   * other bites of codes from here and there
   * mongoose HTTP server : http://code.google.com/p/mongoose/
   * linenoise minimal "readline" : https://github.com/antirez/linenoise

Wiki
____
https://github.com/miellaby/entrelacs/blob/wiki/ProjectHome.md

Install & test:
______________
make tests ; # compile and tests everythings (type Ctrl+D when in the shell)
make help ; # see other build commands

Additional configuration:
________________________
- See options in servers.c

Exe files :
________________
bin/libentrelacs.a ; # static library for the entrelacs "core"
bin/libentrelacs.so ; # dynamic library
bin/entrelacsd ; # entrelacs HTTP server
repl/bin/entrelacs ; # simple REPL binary
bin/cli.sh ; # simple script to query the server (depends on curl CLI)
tests/testshell ; # minimalist REPL
web-terminal/fill-demo.sh ; # fill up the demo context via cli.sh call
