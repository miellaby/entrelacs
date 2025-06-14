Please find herein an overview of an [Entrelacs System](EntrelacsSystem.md) architecture.

<h4>Arrow Space</h4>

<img src='pictures/mem0.png' align='right'>

A major component of such a system is the <a href='ArrowsSpace.md'>Arrow Space</a>.<br>
<br>
It's simply a device to store arrows.<br>
<br>
<h4>Primary machine and language</h4>
The system also provides a default evaluation machine, namely the <a href='EntrelacsAbstractMachine.md'>Entrelacs abstract machine</a>, which evaluates programs made in the <a href='EntrelacsLanguage.md'>Entrelacs Language</a>. Naturally, both programs and machine states are made out of arrows.<br>
<br>
<h4>System boundaries</h4>

The currently developed prototype stands as a <a href='EntrelacsServer.md'>software server</a> on top of a GNU/Linux hosting system.<br>
<br>
This server publishes A COMET/REST API for networked interaction. A C API is also planed to allow native code execution within the system. See the EntrelacsPrototypeArchitecture for details.<br>
<br>
<h3>Other questions?</h3>
See the <a href='FAQ.md'>FAQ</a>.
