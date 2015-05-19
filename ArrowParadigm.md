# Introduction #

The way current computer systems manage information is shaped by the [Box/Value paradigm](BoxValueParadigm.md) which consists in representing information as graphs of boxed data values.

<img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/versus.png' />

The [Entrelacs Manifesto](http://code.google.com/p/entrelacs/) proposes to build up a brand new computing stack based upon a radically different paradigm introduced hereafter: the _**Arrow paradigm**_.

#### At a glance ####
  * no more than one building block to _tell_ everything: the **arrow**
  * an arrow is an oriented pair of ... arrows. That's all.
  * an arrow is considered in its "canonical" or "unique" form. Every definable arrow is mapped to at most one physical representation per system.
  * an arrow is considered "immutable". The physical representation of an arrow can't be modified once placed.
  * an arrow is "connected" with its ends. The system may browse these connections in both ways so to explore all the information attached to every arrow.
  * one doesn't need anything else than arrows to represent information. The old "nodes vs. edges" duality from graph theory is rejected.
  * But raw data can still be considered as some sorts of arrows, typically re-entrant self-defining arrow constructs named _entrelacs_.

## Arrow ##
<img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/arrows.png' align='right'>

<blockquote><h3>a pair</h3>
An <i>arrow</i> is basically a pair of arrows. <b>Arrow ends are not nodes but other arrows</b>. <a href='http://en.wikipedia.org/wiki/Graph_theory'>Graph theory</a><sup>W</sup> doesn't cover arrow made constructs.</blockquote>

<blockquote><h3>oriented</h3></blockquote>

<blockquote>The pair (A,B) is distinct from (B,A).</blockquote>

<blockquote>Note that one may dismiss this requirement and build back oriented pairs by combining non-oriented pairs.</blockquote>

<blockquote><h3>unique</h3>
Syn. canonical, normed, hashed.</blockquote>

<blockquote>An arrow-based system stores only one physical representation of the arrow (A,B) from A to B, whatever A and B two known arrows.</blockquote>

<blockquote>Self-referring sets of arrows -that is <i>entrelacs</i> as introduced hereafter- are also uniquely identified.</blockquote>

<blockquote><h3>immutable</h3></blockquote>

<blockquote>An arrow is to be considered as a pure mathematical object. And in the same way that one can't "modify" a natural number like 2, one can't <i>modify</i> an arrow within a system</blockquote>

<blockquote>The system dynamic goes by assimilating new arrows and forgetting old ones.</blockquote>

<blockquote><h3>connected</h3></blockquote>

<blockquote>Inside a given system, a certain arrow may be connected to many <i>children</i> arrows, that is arrows whose one of both ends are the considered arrow.</blockquote>

<blockquote>The system must be able to efficiently browse all these children as it forms <i>emerging information</i>. It's the basis of a new kind of system abilities.</blockquote>

<h2>Wordlist</h2>

<img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/arrow1.png' align='right' />

Each arrow is an oriented pair of arrows.<br>
<ul><li>The first arrow is the <i>tail</i>.<br>
</li><li>The last arrow is the <i>head</i>.<br>
</li><li>Tail and head are <i>ends</i>.<br>
</li><li>An arrow <i>goes out</i> its tail.<br>
</li><li>An arrow <i>comes into</i> its head.<br>
</li><li>From its tail point of view, an arrow is an <i>outgoing</i> arrow.<br>
</li><li>From its head point of view, an arrow is an <i>incoming</i> arrow.<br>
</li><li>Incoming and outgoing arrows are <i>children</i>.<br>
</li><li>Children, children of children, and so on, are <i>descendants</i>.<br>
</li><li>Ends, ends of ends, and so on, are <i>ancestors</i>.</li></ul>

<h2><i>Entrelacs</i></h2>

An <i>entrelacs</i> is a set of intricated arrows forming an unique discrete connected construct. It means that any arrow from the set is the descendant/ancestor of all the other arrows of the set.<br>
<br>
Definition:<br>
<br>
<blockquote>A set of arrows E forms an entrelacs if and only if<br>
<ul><li>whatever 2 arrows A and B in E,<br>
</li><li>A is a descendant of B</li></ul></blockquote>

One characterizes an entrelacs according to its topology, that is how many arrows it contains and how these arrows are connected together. One considers that <b>entrelacs which are isomorphic are identical</b>.<br>
<br>
<br>
<br>
<table><thead><th> Examples: </th><th> <img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/eve.png' /> </th><th> <img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/composed1.png' /> </th></thead><tbody>
<tr><td>  </td><td> <a href='http://en.wikipedia.org/wiki/Orobouros'>Orobouros</a><sup>W</sup> which has no ancestor but itself </td><td><p align='center'> 2 entrelacs linked by a regular arrow</p> </td></tr></tbody></table>

<h3>Entrelacs-Atoms Correspondence</h3>

Entrelacs act as atomic pieces of information.<br>
<ul><li>They got finished boundaries<br>
</li><li>One can't identify smaller isolated components in them.<br>
</li><li>But one can <b>enumerate</b> them<br>
</li><li>They are no reason to discern its inner component. For example, Yin and Yang are not discernible from each other (what's true for one is true for the other). One may see such an entrelacs as a unique arrow.</li></ul>

To conclude, <i>entrelacs</i> are equivalent in all respects to <b>atoms</b> like those found in <a href='http://en.wikipedia.org/wiki/S-expression'>S-expression</a><sup>W</sup>.<br>
<br>
<h2>Practical considerations</h2>

<h3>Assimilation process</h3>

All system inputs (network events, user actions, ...) must be <i>assimilated</i> into arrows before being processed by the system.<br>
<br>
This "Assimilation" process also occurs when low-level processes produce new data, e.g. numbers obtained by mathematical operations.<br>
<br>
Arrows uniqueness is enforced during assimilation by checking that no previous representation of each arrow exists in the storage space before adding one to it.<br>
<br>
<h3>Rooting</h3>

From a theoretical perspective, the whole knowledge of a given system at given time might be defined as a single arrow <i>S</i>. <i>S</i> definition would include all other known arrows. It would be the only movable "variable" of the computer system.<br>
<br>
But, for practicability, one represents the system top-level knowledge as a mutable set of arrows, namely the <i>rooted arrows</i>.<br>
<br>
A rooted arrow is decorated with a "root" flag. It means that this arrow is considered "true" in the top-level context of the considered system.<br>
<br>
<p align='middle'><img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/rooted1.png' /><br /><u>some rooted arrow.</u><br />(check mark notation)</p>

Arrow uniqueness induces that an arrows may be often simultaneously rooted and an ancestor of rooted arrows.<br>
<br>
Thus, a system-level garbage collector must reclaim storage space by removing physical representations of arrows which are neither a <i>rooted</i> arrow nor an <i>ancestor</i> of any other rooted arrow.<br>
<br>
<h2>Handling of complex objects</h2>

From a theoretical perspective, <i>arrows</i> are the only building blocks needed to represent information.<br>
<br>
However, a realist system should handle raw data -like binary strings- as well.<br>
<br>
Thanks to the <a href='AtomEntrelacEquivalency.md'>Entrelacs-Atoms Correspondence</a>, this doesn't deny the initial paradigm as <i>data</i> eventually correspond to arrows.<br>
<br>
<img src='https://github.com/miellaby/entrelacs/blob/master/doc/pictures/bin1.png' align='right' />

More generally, an arrow based system may be extended to take into consideration advanced building blocks like <i>tuples</i> or <i>bags</i> <b>as long as</b> these complex objects are handled exactly in the same way as their equivalent arrow constructs, especially in terms of uniqueness, immutability, and connectivity.<br>
<br>
For instance, when assimilating the "hello world" character string, an arrow based system must ensure that:<br>
<ul><li>there is no more than one copy of the string in the whole storage space,<br>
</li><li>the string can't be modified in place,<br>
</li><li>one may browse all the arrows connected to this string, like all the programs which cite "Hello world".</li></ul>

<h2>Orthogonal persistence</h2>

A practical arrow-based system should ensure the <a href='http://en.wikipedia.org/wiki/Orthogonal_persistence'>orthogonal persistence</a><sup>W</sup> of arrows. It means that neither programs nor users are concerned by information persistence. The management of RAM, disks, caches, pages, and all the related garbage collection mechanisms should be totally transparent.<br>
<br>
<h2>Knowledge representation with arrows</h2>

To get useful information structures with arrows, one may adapt many classical meta-models or even invent new ones. See ArrowModeling for further readings.<br>
<br>
<h2>Similarity with existing concepts</h2>

Arrows are somewhat similar to <a href='http://en.wikipedia.org/wiki/Cons'>cons cells</a><sup>W</sup>, especially "hons" obtained by <a href='http://en.wikipedia.org/wiki/Hash_consing'>"hash consing"</a><sup>W</sup>. A definition of "hons" is given by the documentation of  <a href='http://www.cs.utexas.edu/~moore/acl2/current/HONS.html'>the ACL2 system</a>.<br>
<br>
All in all, the Entrelacs manifesto proposal simply consists in performing system-wide <i>hash consing</i>, combined with transparent persistence and indexation.<br>
<br>
The only invention of this paradigm might be that one considers non reducible systems of equations as materials to "bootstrap" the information system. For example the equation<br>
<i>x = x x</i> defines "Orobouros" while the equations system { <i>x = y . y & y = x . x & x != y</i> } defines "Yin-Yang".<br>
<br>
It's like the grammar of "lambda calculus" without free variables as one may use lambda abstractions as constants and alpha-equivalence to check constant equality.<br>
<br>
This approach is motivated by the desire to merge relationships and nodes into a single concept. It leads to design a system where information is homogeneous and "scale-invariant" on the "meta" complexity scale.
