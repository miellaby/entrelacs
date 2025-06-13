## introduction ##
An _"Arrows Space"_ (AS) refers to an abstract device to store arrows structures involved in the [Entrelacs Paradigm](ArrowParadigm.md). It's the key component of an [Entrelacs System](DesignIntroduction.md).

Please note one may simulate an Arrows Space with [paper and pencil](PenAndPaperReferenceDesign.md) .

Maybe one day, an AS will be designed as a dedicated piece of hardware. If so, it won't necessarily consist in a bank of data buckets like all other data storage so far.

But for the moment, we introduce the AS as a a piece of software leveraging on traditional volatile and mass memory hardware.

<p align='middle'><img src='pictures/mem0.png' /><br />AS artistic view.<br>Arrows are basically stored as pairs of pointers<br>
<br>
<h2>AS requirements summary</h2>
One requires from an AS<br>
<br>
<ul><li>Orthogonal persistence<br>
<blockquote>The AS must leverage on all persistent and volatile physical memory levels and flatten them down to a single homogeneous space.</blockquote></li></ul>

<ul><li>De-duplication<br>
<blockquote>The AS must ensure each mathematically definable arrow is never stored more than once in the whole storage space.</blockquote></li></ul>

<ul><li>Full connectivity<br>
<blockquote>The AS must store enough connectivity data to allow efficient retrieving of the ends and the <i>children</i> of any arrow.</blockquote></li></ul>

<ul><li>Garbage collection relatively to a <i>root</i> referential<br>
<ul><li>The AS must manage the <i>root</i> boolean property of every stored arrow. The set of <i>rooted</i> arrows forms an unique absolute referential.<br>
</li><li>The AS must preserve rooted arrows and their ancestors from deletion, while arrows which are neither a rooted arrow nor an ancestor of a rooted arrow must be transparently removed from the storage.</li></ul></li></ul>

<ul><li>Binary strings and other complex objects native support<br>
<ul><li>The AS may allow raw data (binary strings) native storage.<br>
</li><li>The AS may allow tuples (ordered set of arrows) native storage.<br>
</li><li>Though such objects must be treated like their equivalent arrow made constructs (entrelacs) in terms of uniqueness, immutability, and connectivity.</li></ul></li></ul>

<h2>API overview</h2>
The ArrowSpaceInterface sums up the <i>arrows space</i> behavior to traditional software developers.<br>
<br>
<h2>How to reach these goals within existing hardware</h2>
<h3>Prevent duplicates during arrows assimilation</h3>

The <i>Arrows Space</i> is filled up by converting data into arrows. This conversion process is called <i>assimilation</i>. It takes place:<br>
<ul><li>when data -like user inputs- come from the outside.<br>
</li><li>when some internal computation produces new arrows.</li></ul>

Each new arrow is assimilated from its farthest ancestors, that is <i>entrelacs</i>. One first assimilates entrelacs, then entrelacs' children arrows, then their grand children, and so on up to the overall assimilated arrow.<br>
<br>
To ensure each mathematically definable arrow is stored at most once, the assimilation process may operate the whole mass storage device as a giant content-indexed open-addressed reentrant hash-table.<br>
<br>
Main steps of an arrow <i>assimilation</i>
<ol><li>hashing the arrow definition to get a default location.<br>
</li><li>looking for the existing singleton at the default location.<br>
</li><li>putting the arrow definition here if missing.<br>
</li><li>managing conflicts through <i>open addressing</i> as introduced hereafter.</li></ol>

<h3>Hash everything !</h3>

The first step of arrow assimilation process consists in computing a default location where to probe the arrow singleton and store it if missing.<br>
<br>
To do so, one computes an hash code from the considered arrow definition.<br>
<ul><li>Concerning a regular arrow, since it's basically a pair of ends, one computes its default location by hashing its both ends addresses.<br>
</li><li>Concerning raw data, the AS should hash the whole object body.<br>
<ul><li>For a very long binary object, a <b>cryptographic hash function</b> is used to get both a default location and an unique signature<br>
<ul><li>The signature is stored next to the object body so to quickly identify a singleton.<br>
</li><li>A cryptographic hash also avoids <i>data cluttering</i>.<br>
</li></ul></li><li>For a shorter binary object, a simpler <b>checksum</b> is used.<br>
<ul><li>This checksum doesn't uniquely identify the singleton. One must fully compare the candidate singleton to the assimilated object.</li></ul></li></ul></li></ul>

<h3>Solve conflicts by Open Addressing</h3>

Once truncated modulo the total mass storage space size, an hash code defines a valid default arrow location.<br>
<br>
In case of conflict with existing unrelated arrows, one shifts this default location by some offset. If there's still a storage conflict, one shifts the location again, and so on.<br>
<br>
Looking for an already stored arrow consequently requires searching in several locations starting from a default one:<br>
<ul><li>if one finds the expected arrow at the considered address ==> hit!<br>
</li><li>if one finds an empty location ==> miss! This empty location is used to store the missing arrow.<br>
</li><li>if one finds something else, then one keeps on searching at a nearby location.</li></ul>

This approach is called <a href='OpenAddressing.md'>open addressing</a> when applied to hash tables. The default storage location is named an <i>open address</i> and the search process described above is called <i>probing</i>.<br>
<br>
<br>
<h3>Perform Orthogonal Persistence</h3>

As a whole, the AS is a single persistent memory device merging all the memory levels of the hosting system. In other words, the AS stores arrows into a mass storage device acceded through a massive RAM-based cache. This cache operates almost all the available fast-access volatile memory resources.<br>
<br>
<h2>Anticipated prototype</h2>

All the strategies introduced above are applied to the currently designed <a href='ArrowsSpacePrototype.md'>AS prototype</a>. See this page for more details.
