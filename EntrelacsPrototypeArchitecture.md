For now, the [Entrelacs System](EntrelacsSystem.md) prototype at <https://github.com/miellaby/entrelacs> stands as a software stack on top of a POSIX system like GNU/Linux.

It includes these components:
  * A library allowing to
    * access the [Arrow Space](ArrowsSpace.md),
    * run the [Entrelacs machine](EntrelacsAbstractMachine.md).
  * An [HTTP application server](EntrelacsServer.md) implementing an HTTP/REST API based on the library.
  * A web based terminal for the end user.

There are two API to the [Arrow Space](ArrowsSpace.md) and its related [Entrelacs Machine](EntrelacsAbstractMachine.md):
  * The C API of the [entrelacs library](EntrelacsLibrary.md)
  * The HTTP/REST network API of the [EntrelacsServer](EntrelacsServer.md).

The C API allows to inject native code in the Entrelacs machine as new operators. The plan is to allow the inclusion of trusted binary executables by assimilating Blobs in the Arrow Space.

### The Entrelacs Library

The system software layer consists in a thread-safe C library ([EntrelacsLibrary](EntrelacsLibrary.md)) that provides:

* operators to define, root, and browse arrow structures stored in a single persistence file.
* a **very** basic functional language interpretor, known as the [Entrelacs Machine](EntrelacsAbstractMachine.md), that runs arrow-based code and whom evaluation state is itself an arrow.

### The HTTP server

The [Entrelacs server](EntrelacsServer.md) on top of the Entrelacs library allows clients and end-users to share the same Arrow Space via HTTP.

### Various clients

* the artsy Entrelacs web terminal is connected to the HTTP server.
* The REPL client also directly accesses to the persistence file.
* The future Entrelacs FUSE file system allows to mount the Arrow Space in the host file system and see arrows as an infinite structure of folders and files.
