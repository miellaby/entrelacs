Entrelacs System first prototype.

Visit <https://miellaby.github.io/entrelacs/> for more information.

Code released under GPLv3 <http://www.gnu.org/licenses/gpl.html>

Project Pages:
_____________
<https://miellaby.github.io/entrelacs/>

Install & test:
______________
make tests ; # compile and tests everythings (type Ctrl+D when in the shell)
make help ; # see other build commands

Additional configuration:
________________________
- See options in servers.c

Binary files:
____________
bin/libentrelacs.a ; # static library for the entrelacs "core"
bin/libentrelacs.so ; # dynamic library
bin/entrelacsd ; # entrelacs HTTP server
repl/bin/entrelacs ; # simple REPL binary
bin/cli.sh ; # simple script to query the server (depends on curl CLI)
tests/testshell ; # minimalist REPL
web-terminal/fill-demo.sh ; # fill up the demo context via cli.sh call

Credits:
_______
   * sha1.c / sha1.h : immoderate copy of PolarSSL sha1 functions. <http://www.polarssl.org/>
   * log.c / log.h : shameless MiniDLNA source copy
   * mongoose HTTP server : <https://github.com/cesanta/mongoose>
   * linenoise minimal "readline" : <https://github.com/antirez/linenoise>
   * other bites of codes from here and there

