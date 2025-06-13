### Preamble ###

Applied computer sciences help improving data structures.

However, no study has yet put into question the fundamental _paradigm_ which governs how such data structures are mapped onto concrete storage space.

Consequently, **all** digital and non-digital systems up to now follow a single paradigm, derived from _Writing_. As a side effect, all these systems are more or less modeled after _graph theory_.

Because the [box/value paradigm](BoxValueParadigm.md), as we'll call it here, is the only paradigm an human being is accustomed to, neither scientists nor engineers have deeply explore a true alternative.

---

# The Entrelacs manifesto #

The **Entrelacs manifesto** claims one **can** and one **should** design a computer system based upon an alternate information paradigm.

Anyone interested in this objective can freely adhere and relate their work to this manifesto.

More specifically, the **manifesto** makes the following claims:

1. **The only paradigm in use so far to store information within computers is the [box/value paradigm](BoxValueParadigm.md). Unfortunately, it's a fundamental _[abstraction inversion](http://en.wikipedia.org/wiki/Abstraction_inversion)_**  

2. **The _Entrelacs Paradigm_ is the fixed abstraction for information digitalization**  
   It consists in promoting **arrows** as the **first** and **only** citizen class of the whole information system.  
   Arrows are **recursively** defined as **singletons of immutable connected ordered pair of arrows**.  
   In this paradigm, all data structures are reimagined as constructs made out of _arrows_, up to elementary information units like _vertex_ and _values_.  
   [Read more about the Entrelacs paradigm ...](ArrowParadigm.md)

3. **It's possible to design an entirely new knowledge system designed after the _Entrelacs paradigm_**  
   as demonstrated by a simple ["pen & paper" experiment](PenAndPaperReferenceDesign.md).

4. **It's possible to code a reasonably efficient _software implementation_ of such a knowledge store, known as an [_Arrow Space_](ArrowsSpace.md)** on top of existing hardware platforms.

5. **Future Computing Environments that will leverage _Arrow Spaces_ will develop a whole new range of [native abilities](EntrelacsCapabilities.md).**
