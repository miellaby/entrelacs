Entrelacs might eventually become a real Operating System. But for the moment, the Entrelacs prototype stands as yet an other virtual machine on top of an existing software stack; typically a GNU/Linux system.

The current prototype includes these components:
  * The [Arrow Space](ArrowsSpace.md).
  * The [Entrelacs machine](EntrelacsAbstractMachine.md).
  * An [HTTP/REST server](EntrelacsServer.md).
  * A web based terminal (for the end user)

There are two public interfaces which gives access to both the ArrowsSpace and the EntrelacsAbstractMachine.
  * The C API of the [entrelacs library](EntrelacsLibrary.md)
  * The HTTP/REST network API of the [EntrelacsServer](EntrelacsServer.md).

In addition, a future API should permit the addition of native code to the Entrelacs machine. This API might been comprehended by the Entrelacs System itself, so rooting a particular arrow including a binary executable as a Blob might assimilate this binary as a system extension.