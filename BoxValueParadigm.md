## Introduction to the problem ##

A large part of computer sciences consists in studying  [data structures](http://en.wikipedia.org/wiki/Data_structure)<sup>W</sup>.

But how do we map data structures onto a physical support in first place?

For example, let's consider a set of statistics organized in a [tree](http://en.wikipedia.org/wiki/Tree_(data_structure))<sup>W</sup>. How are we going to put this structure onto a computer RAM and drives, or even a piece of paper?

<img src='http://entrelacs.googlecode.com/svn-history/r489/trunk/doc/pictures/box_paradigm.png' align='right' />

## The box/value framework ##

The usual practice consists in dividing the storage space into boxes so to put values into. Boxes are often nested into each others, so that a given box and its value can be comprehended as a gathering of smaller boxes. Also one generally use some values to identify and locate existing boxes in the storage space. They are _pointers_, _indices_, _names_, or any other parts of box identifiers.

One will call this habit the _box/value framework_, or the _box/value paradigm_. We could have named it the _box/value meta-structure_ as well, because it deals with the canonical structure used to represent every other structure components: graph vertices and edges, table records and indices, etc.

Within a computer, values are binary representation of natural numbers. Every piece of information can eventually be mapped to some natural numbers. The box/value paradigm puts the stress on these values, that one usually calls **data**.

By following this paradigm, we are typically going to map our tree into boxes, every box containing one tree node data and some numbers referring to child node boxes.

<img src='http://entrelacs.googlecode.com/svn-history/r489/trunk/doc/pictures/Writing.png' align='right' />

## box/value paradigm = Writing ##

But why doing so? Why programmers and scientists have so rarely looked for alternatives?

One may presume it's due to how _Speech_ and _Writing_ have forged our mind.

_Writing_ is all about building up a hierarchy of nested boxes -sentences, words...- starting from signs, like letters or ideograms.

See, textual units correspond to physical representation of concepts. And as information is serialized into sentences, word repetition is the norm.

> Luckily, some constructs reduce redundancy by pointing other parts of a speech and referring to previously introduced concepts; They act like _pointers_ and _indices_ of computer data structures.

All in all, _Writing_ is the typical implementation of the box/value paradigm. This process is so deeply rooted in our mind that it forms a mental lock making side-thinking excessively difficult.

## box/value paradigm = graph theory ##
> <img src='http://entrelacs.googlecode.com/svn-history/r489/trunk/doc/pictures/Graphe.png' align='right' />

The box/value paradigm is slightly related to [graph theory](http://en.wikipedia.org/wiki/Glossary_of_graph_theory)<sup>W</sup>.

A _graph_ consists in two types of elements, namely _vertices_ and _edges_. So does the box/value paradigm: boxes are _vertices_ and values act as _edges_ linking these boxes to each others.

To say it clearly: **as long as an information model relays on a _graph_, it's the implementation of the _box/value_ paradigm.**

So every time a new technology improves the box/value system, such as [triple stores](http://en.wikipedia.org/wiki/Triple_store), one must keep in mind that there's no paradigm shift. These technical improvements eventually don't solve most of the issues introduced herein.

## Issues with the box/value paradigm ##

  * **Redundancy**

> The box/value paradigm leads to map the same atomic concept again on many places. And recursively, any gathering of concepts will also tend to be repeated ad nauseam.

> For example, if our data tree contains several references to the natural number 2, this number will be repeated as is in as many boxes. Tree branches based on the same couple of natural numbers will also be repeated again and again.

> Our habits make us think that such a redundancy is necessary. Actually, it's not. Quite the contrary, dispatching many representations of the same concept among the storage space prevents software to exhibit a lot of useful behaviors.

> The issue is not so about room sparing. It's all about all these jobs that one can't ask for a computer without designing first a very complex and very resource-costly piece of software.

> <u>Concrete example</u>: You can't ask for your computer when and how it makes use of a given PI number approximation, or some English word it knows. Search engines included in modern desktop environments nowadays form a very incomplete solution to this problem.

  * **Compartmentalization**

> The box/value paradigm leads to dispatch information into separate heterogeneous storage spaces.

> A regular operating system gives tools to browse the structure of the biggest boxes it is responsible for, typically file systems, but not the inner structure of these boxes, which depend on the applications which manage them.

> Thus, neither a human or software agent can fully browse and operate the content of a computer. One can't neither build new information nor produce new behaviors by _cross-cutting_ information contained in all these closed boxes.

> <u>Concrete example</u>: A simple picture manager will manage user pictures in well-known folders. But it won't get access to pictures embedded in other applications, like those stored or archived by other picture managers or mail agents. Above all, no software today can simply access to RAM-based pictures currently displayed by opened applications, whatever the access rights given by its user.

  * **Duality**

> The box/value paradigm is fundamentally dual.

> _Boxes_ + _vertices_  on one side, _values_ + _edges_ on the other side.


> This fundamental duality propagates itself everywhere (see also the data / meta-data paragraph hereafter). It eventually shapes the relationships between computers and their users, by oddly separating programmers from regular users.

> This separation is so common that it happens to feel natural and necessary. And yet, its first effect is to severally curb computers abilities.

> <u>Concrete example</u>: programmers on one side, and end-users on the other side. Maybe fine for the software industry; but is it always fine to you?

  * **Rigidity**

> As values and boxes are most of the time gathered into larger boxes, the box/value way has a negative impact on information plasticity.

> Bringing changes to information continuously obliges to enlarge existing boxes, and shift data to get room. It leads programs to be excessively complex due to costly [serialization and deserialization](http://en.wikipedia.org/wiki/Serialization)<sup>W</sup> processes which aim to get back a bit of this lost plasticity.

> Despite these huge and costly efforts in software development, most information that resides in a computer -the programs themselves in first place- can't be modified by "John Doe" user to better fit his needs.

> <u>Concrete example</u>: you can't modify your preferred application in any other aspect than those anticipated by its developers. **Access to source codes** won't even help much!

  * **Opacity**

> With the box/value paradigm, one gives meaning to data by considering the boxes they belong to. For a given value at a given location, one must take into consideration its location relatively to its parent box, which is itself positioned in its grandparent box, and so on up to the underlying material.

> As said before, boxes boundaries are generally indistinguishable from the outside. So in most cases, one can't even characterize the upper box containing a given value at a random place. This opacity prevents to recover the hierarchy of contexts needed to find the meaning to a found value.

> <u>Concrete example</u>: Software can't be studied and modified without original source code. Also, data buried in user files can't be browsed without the specially created programs which manages these data. Extracting information from _raw_ data out of any context is called data _forensics_ and is more considered as a kind of hacking.

  * **Broken information retrieval**

> A box/value based information storage only guarantees that information may be browsed in one direction: from the highest boxes to the lowest values. It can't be simply browsed in the opposite direction. A program typically can't fetch the references up to a known box.

> <u>Concrete example</u> In most file systems, there is no simple way to know which directories links to a given file. But What's worse is how most file references in programs consist in regular strings -file paths- that are lost in code and data with no way to efficiently retrieve and change them.

  * **No meta**.

> With the box/value paradigm, a box content is either a reference to a box -though there is generally no way to know it- or not. So one easily represents either the concept of "such a value" or "such a box".

> But this way to store information  can't simply represent a concept like _"such a value in such a box"_, or _"such a box in such a box"_ while such concepts are valid and useful pieces of information that humans can easily understand and handle.

> That's why existing computer based systems are unable to handle its own knowledge at an higher _level of abstraction_, i.e. at a _meta level_. It might be the biggest issue of the box/value paradigm.

> Sure, many popular applications claim they manage _meta-data_ about various things. But keep in mind that these applications only consider **one** level of "meta". They are notably incapable of managing meta-data attached to these meta-data.

> <u>Concrete example</u>: A regular file system doesn't give a simple way to store the time when file access policies change (meta-data about meta-data).

  * **Weak and breakable**.

> Most box/value based storages doesn't offer simple generalized ways to link existing boxes with each others.

> Sure one may create a box including a reference to another box, but this reference will be broken or outdated if the box moves within the storage space or if its content evolves. In such conditions, no software can ensure satisfying system level consistency.

> <u>Concrete example</u>Most photo suites attach information to pictures by setting up a database indexed by file paths. These tenuous links are broken by renaming or moving original picture files. In regular operating systems, program can't obtain strong and reliable references to existing files so to attach information on them from the outside. Once files are moved, renamed, deleted or modified in place, these bindings got wrong.

  * **Lack of relativity**

> Within a box/value storage space, some boxes may store other boxes references as values. This architectural pattern leads to hierarchies of contexts embedded into each others -like folder trees-. In other words, it leads to store graphs of graphs, that is _meta-graphs_.

> This is the usual way to implement user profiles, accounts, workspaces, and such. That's also the typical solution to store _meta-data_ referring existing contents.

> But in the same way than graph theory doesn't allow edges ends to be other edges, the box/value framework provides no way to represent a concept like "such a value in such a box".

> So while data may be defined relatively to some context, the meaning of such a data considering such a context can't be stated and devised relatively to a higher level of abstraction.

> At the very end, the computer behavior turns out to be mostly _hardcoded_ because there is no way to store different interpretations of existing pieces of informations.

ArticleCompleteness: 70%