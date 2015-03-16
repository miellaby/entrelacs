### Preamble ###

Applied computer sciences help improving data structures.

However, no study has yet put into question the fundamental _paradigm_ which governs how such data structures are mapped onto concrete storage space.

Consequently, **all** digital and non-digital systems up to now follow a single paradigm, derived from _Writing_. As a side effect, all these systems are more or less modeled after _graph theory_.

Because the [box/value paradigm](BoxValueParadigm.md), as we'll call it here, is the only paradigm an human being is accustomed to, neither scientists nor engineers have deeply explore a true alternative.


---


# The Entrelacs manifesto #

The **Entrelacs manifesto** claims one **can** and one **should** design a computer system based upon an alternate information paradigm.

Anyone interested in this objective can freely adhere and relate his work to this manifesto.

More specifically, the **manifesto** makes the following claims:

<ol>
<blockquote><li><strong>The current paradigm in use regarding information storage within computers is an <i>abstraction inversion</i> <sup><a href='http://en.wikipedia.org/wiki/Abstraction_inversion'>wiki def</a></sup></strong>
<blockquote><p> <a href='BoxValueParadigm.md'>Read more about the box/value paradigm ...</a>
</blockquote><li><strong>Fixing this abstraction inversion leads to the <i>Entrelacs Paradigm</i></strong>
<blockquote><p> It consists in promoting <b>arrows</b> to be the <b>first</b> and <b>only</b> citizen class of the whole information system.<br>
<p> Arrows are <b>recursively</b> defined as <b>singletons of immutable connected ordered pair of arrows</b>.<br>
<p> In this paradigm, old concepts, like <i>vertex</i> or <i>values</i>, are considered as various constructs made out of <i>arrows</i>.<br>
<p> <a href='ArrowParadigm.md'>Read more about the Entrelacs paradigm ...</a>
</blockquote><li><strong>An information storage system designed after the <i>Entrelacs paradigm</i> is feasible</strong>
<blockquote><p>as demonstrated by a simple experiment with paper and pencil.<br>
<p><a href='PenAndPaperReferenceDesign.md'>Read more about the "pen &amp; paper" reference design ...</a>
</blockquote><li><strong>Coding a <u>software implementation</u> of such a storage system is possible</strong>
<blockquote><p>with reasonable performances on existing hardware plateforms.<br>
<p><a href='ArrowsSpace.md'>Read more about arrow spaces ...</a>
</blockquote><li><strong>Computing environments on top of <i>arrows spaces</i> will feature a whole new range of native abilities.</strong>
<blockquote><p><a href='EntrelacsCapabilities.md'>Read more about native abilities of "Entrelacs Systems"...</a>