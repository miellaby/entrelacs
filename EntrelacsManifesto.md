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

1. **The current paradigm in use regarding information storage within computers is an _abstraction inversion_**  
   [wiki def](http://en.wikipedia.org/wiki/Abstraction_inversion)  
   [Read more about the box/value paradigm ...](BoxValueParadigm.md)

2. **Fixing this abstraction inversion leads to the _Entrelacs Paradigm_**  
   It consists in promoting **arrows** to be the **first** and **only** citizen class of the whole information system.  
   Arrows are **recursively** defined as **singletons of immutable connected ordered pair of arrows**.  
   In this paradigm, old concepts, like _vertex_ or _values_, are considered as various constructs made out of _arrows_.  
   [Read more about the Entrelacs paradigm ...](ArrowParadigm.md)

3. **An information storage system designed after the _Entrelacs paradigm_ is feasible**  
   as demonstrated by a simple experiment with paper and pencil.  
   [Read more about the "pen & paper" reference design ...](PenAndPaperReferenceDesign.md)

4. **Coding a _software implementation_ of such a storage system is possible**  
   with reasonable performances on existing hardware platforms.  
   [Read more about arrow spaces ...](ArrowsSpace.md)

5. **Computing environments on top of _arrow spaces_ will feature a whole new range of native abilities.**  
   [Read more about native abilities of "Entrelacs Systems"...](EntrelacsCapabilities.md)