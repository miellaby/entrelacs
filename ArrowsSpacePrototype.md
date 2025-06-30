# The Arrow Space Prototype

> This page may require house cleaning since the recent migration to a new data model

This page describes the design of the [Arrow Space](ArrowsSpace.md) as it takes place in the [Entrelacs System prototype](EntrelacsPrototypeArchitecture.md).

Source code:
<https://github.com/miellaby/entrelacs/blob/master/space.c>

## Mass storage

The _Arrow Space_ prototype aims to map __abstract arrows__ into a single _persistence file_ of the host system during a process called __assimilation__. This file is operated as a __giant hybrid double-hashed-and-coalesced open-addressing hashtable__.

The persistence file is divided into individual _slots_ also known as _cells_ and identified by _cell addresses_.

As stated by the reentrant definition of the [Entrelacs Paradigm](ArrowParadigm.md), a regular arrow is a a pair of arrows. Such an arrow is consequently stored as a pair of addresses within a single cell, in addition with metadata.

## Volatile storage

RAM is used as a massive direct-mapped cache of cells on top of the persistence file.

When needed, arrows climb up to a level 1 (RAM) memory device. This memory is used as a fine-grained direct-mapped cache of level 0 (disk) cells.

See the page [Arrow Cache](ArrowsCache.md) for further details and <https://github.com/miellaby/entrelacs/blob/master/mem1.c> for the actual code.

## Pairs and Atoms

The _Arrow Space_ aims to store atomic arrows, also known as _Entrelacs_, in addition with regular _pair of arrows_.

One will distinguish atomic data depending on their sizes.

* _smalls_: From 1 to 11 bytes
   A _small_ is a piece of raw data small enough to fit in a single cell.
* _tags_: From 12 to 99 bytes
   A _tag_ is a binary string that is too short to be hashed with a cryptographic function. Instead, a much simpler checksum function is used. In addition the binary string is stored in chained cells of the main storage.
* _blobs_ : 100 bytes and more
   A _blob_ is a sufficiently long binary object for a cryptographic footprint to be computed. Its body is stored out of the main storage. Its cryptographic footprint is stored in the Arrow Space in place of the whole defintion.

Despite _atoms_ being binary objects, they are handled like arrow constructs, namely _Entrelacs_, which are discrete reentrant structures of arrows. _Atoms_ are consequently self-indexed, deduplicated and fully-connected like their arrow counterparts.

## Open Addressing

One relays on [Open Addressing](OpenAddressing.md) to manage conflicts within the storage space.

When one cannot store an arrow at its expected location because the cell is already occupied, one repeatedly shifts the cell adress by a deterministic offset until a free cell is found.

When looking for a given arrow, one probes the arrow space starting from the expected location and follows the same sequence of shifts until the arrow is found (it's a hit!) or an empty cell is encountered (it's a miss!).

## Hop-o'-My-Thumb algorithm

The _Open Addressing_ approach to hash conflict has a limitation. Clearing a cell can disrupt one or more sequences of occupied cells, which can result in the loss of information.

The retained solution consists in including a dedicated _pebble_ counter into each cell. It is incremented each time the cell is visited while probing for an available location. Conversely, when removing an arrow from storage, the _pebble counter_ of every cell within the probing sequence is decremented.

With the _pebble_ counters in place, it becomes possible to determine when to continue probing for stored objects —specifically, as long as the _pebble_ counter is greater than zero— even if the cell itself has been emptied.

## Connectivity Information Storage

Every arrow is self indexed, as its definition is hashed to determine an _open address_ within the storage space.

In addition, an arrow is indexed by its _tail_ and _head_.

For this purpose, _back-references_ are stored at expected locations related to the arrow _ends_.

These back-references form an index of all _child arrows_ related to a given arrow.

Note these back-references are stored only once the arrow permanently saved to mass storage. A transient arrow may not be indexed.

Arrows back-references are stored in dedicated cells found by shifting the parent address by a _children offset_. A same "children" cell may actually store unrelated back-references. Wrong back-references are filtered out by checking refered arrows. As there is no need to chain children segments in an unambigous way, there is no "jumper" field involved. Children list also don't involve "pebble" counters. However a reference counter is stored in the parent arrow definition so that the search will proceed until all children are found.

_note_ one distinguishes outgoing from incoming back-references. Does it actually worth it?

## Binary strings storage

Binary strings, ranging from 12 up to 99 bytes in length, are stored directly into cells. This includes Blob footprints as well.

To facilitate deduplication, binary strings are prefixed with a checksum. This checksum alone doesn't guarantee identification so the entire string needs to be compared when resolving an atom.

Binary strings are divided into segments and distributed accross multiple cells, each separated by a deterministic offset, similar to the probing process in _open addressing_. However contrarly to cells visited while probing, string segment cells must be unambigously linked together to prevent unrelated data inclusion.

## Chained cells and _jumpers_

A collision resolution technique named _Coalesced hashing_  consists in chaining hash-conflicting keys from the original slot thanks to explicit pointers (indices).

The _Arrow Space_ code uses a comparable technique but for cells that need to be unambiguously linked together. These cells are referred to as _chained cells_ and are used for binary strings storage.

Within a cell sequence, chained cells are connected by _jumper_ counters, which allow for _jumping_ over unwanted data from one cell to the next in the chain.

However, a _jumper_ is not a fully qualified address but an offset multiplier, making it much more space-efficient. It indicates the number of shifts required in the probing sequence to reach the next relevant cell.

## Reattachment cells

A _jumper_ as introduced above is a short-sized data. Its maximal value is low. The probability of many repeated __consecutive__ hash-collisions is supposed to be even lower, as explained latter.

If the number of shifts up to the next cell overflows the _jumper_ definition range, one puts a special _reattachment point_ into the next cell. This allows to unambiguously finds latter the location where the chain resumes when retrieving back the list.

## Overview of stored data

For each arrow, one stores in the main space:

* the arrow definition, that is
  * two arrow adresses for a pair
  * a checksum and a binary string for a tag or a blob cryptographic footprint.
* its root flag (one mutable bit)
* back-references from the arrow ends to the arrow
* etc.

In addition, one stores _blob bodies_ as dedicated files next to the main persistence file.

## Hash functions

One makes use of an hash function to compute an arrow's _open address_ out of its definition.

The hash function depends on the arrow type.

* A binary string_ is hashed with a simple checksum function.
* A _Blob_ is firstly hashed with a cryptographic hash function (such as SHA128) to obtain a unique short string. The resulting _footprint_ is then processed and stored like a binary string.
* A pair of arrows is hashed by combining its parents' respective hash codes.

In summary:

* `_h_(Tag) = checksum(Tag)`
* `_h_(Pair) = combine(h(tail(Pair)), h(head(Pair)))`
* `_h_(Blob) = footprint(Blob)`

Please note the hash function for a regular arrow is based on its parents' open addresses rather than their actual cell adresses. It allows to compute the hash of a given arrow without resolving the actual addresses of all its intermediary ancestors.

## Double Hashing for probing

When storing a new arrow and upon a location conflict, One could decide to look for a free cell immediately following a conflicting cell. But this naive _linear probing_ would rapidly lead to _data cluttering_.

A better way to probe for a free cell or an existing key is to shift from the default location by a variable offset computed by a second hash function. This reduces the risk of aligning occupied cells together in the same probing sequence. This additional strategy is called _double hashing_.

When putting an arrow into the storage space, one computes a big hash code _h_ as described above, then one truncates the hash code modulo 2 twin _prim numbers_ _P0_ and _P1_.

* H % P0 is the open address,
* H % P1 is the probing offset.

_2012 note: the actual prototype also features triangular grow of the offset as probing goes._

## Double Hashing for moving apart chained cells

Binary strings divided into segments stored among the arrow space.

One could fill up consecutive cells to store all the segments of the binary string. But this naive approach would lead to _data cluttering_.

A better approach consists in separating data slices by an offset which differs depending on the data so to minimize cluttering. This is comparable with _Double Hashing_ when probing.

## All hash functions

_OUTDATED DOCUMENTATION. See [actual code](https://github.com/miellaby/entrelacs/blob/master/space.)._

Address are 4 byte length. So are hash codes.

_P0_ and _P1_ are twin prim numbers just above 2^32 (address space size).

The actual memory size is P0. It's the greatest prime under 2^32.

Here are the hash functions currently used by the prototype.

* pair hash: `h(a) = P1 + (tail << 20) + (head << 4) + tail + head` (64 bits)
* blob hash: `checksum(SHA-2(blob))` (the SHA footprint is stored as a tag)
* tag/footprint hash: `h(tag) = checksum(tag)` 64 bits checksum à la <http://www.cse.yorku.ca/~oz/hash.html>
* small hash: `((size << 24) + ((buffer[1] ^ buffer[5] ^ buffer[9]) << 16) + ((buffer[2] ^ buffer[6] ^ buffer[10]) << 8) + buffer[3] ^ buffer[7] ^ buffer[11])` with buffer right-padded with `size` filling bytes
  * the small hash is only 32 bits long
  * it is designed so that one may extract the 3 last bytes of a 11 bytes sized atom.

With the 64bits hash result, one deduces the following values.

* arrow hash: `h & 0xFFFFFFFFU` (h truncated to 32bits)
* open address: `h % P0`
* probing offset: `h % P1` (if 0 => 1)
* string chaining offset: `invert_bytes(arrow hash) ^ data[0..3] ^ data[4..7]`
* children offset: `invert_bytes(arrow hash)`  (or string chaining offset for strings)


## Cell Structure

This is the 24 bytes cell structure and 4 bytes address model used by the actual source code.

```text
 *   Memory device
 *
 *   -------+------+------+------+-------
 *      ... | cell | cell | cell | ...
 *   -------+------+------+------+-------
 *
 *  Cell structure (24 bytes).
 *
 *   +---------------------------------------+---+---+
 *   |                  data                 | T | P |
 *   +---------------------------------------+---+---+
 *                       22                    1   1
 *
 *      P: Pebble counter (1 byte)
 *      T: Cell content type (1 byte)
 *      Data: Cell payload (22 bytes)
 *
 *
```

* _pebble_
  The "pebble" counter of the Little Thumbling's algorithm introduced above.
* _content type_
  It resolves the inner structure of a cell. Note segment cells store different data depending on the chain they belong to.

The _cell content byte_ identifies the type of content in a cell:

* 0: EMPTY: empty cell
* 1: PAIR: a regular arrow
* 2: SMALL ATOM: a _small_ atom (size < 11)
* 3: TAG: A small binary string first segment
* 4: BLOB FOOTPRINT: A tag-like binary string being a _blob_ footprint
* 5: INTERMEDIARY SLICE: An intermediary segment of a string
* 6: LAST SLICE: Last segment
* 7: REATTACHMENT SLICE: Reattachement for peeble overflow
* 8: CHILDREN SLICE: segment of a list of back-references

### Empty Cell (T=0)

```text
   *   +---------------------------------------------------+
   *   |                      junk                         |
   *   +---------------------------------------------------+
   *                         22 bytes
```

### Common Structure of all arrows (T=1, 2, 3 or 4)

```text
  * T = 1, 2, 3, 4: arrow definition.
  *   +--------+----------------------+-----------+----------+----+----+
  *   |  hash  | full or partial def  | RWC0dWnCn |  Child0  | cr | dr |
  *   +--------+----------------------+-----------+----------+----+----+
  *       4                8                4          4        1    1
  *   where RWC0dWnCn = R|W|C0d|Wn|Cn
  *
  *   R = root flag (1 bit)
  *   W = weak flag (1 bit)
  *   C0d = child0 direction (1 bit)
  *   Wn = Weak children count (14 bits)
  *   Cn = True children count (15 bits)
  *
  *   child0 = 1st child of this arrow
  *   cr = children revision
  *   dr = definition revision
```

* _root flag_
  The root flag as a mutable bit

* _children counters_
  A reference counter is added to every arrow definition. To retrieve all the arrow's children, one look at all the _children cells_ in the _child offset_ based sequence of cells starting from the parent address.

* _child0_
  An arrow definition relates with a _list of children_. _child0_ is the younger child (the last assimilated child arrow). The other older children are listed into individual segments stored in separate cells separated by a _children offset_.

### Regular Arrow = Pair of Arrows (T=1)

```text
  *   +--------+--------+--------+-----------+----------+----+----+
  *   |  hash  |  tail  |  head  | RWC0dWnCn |  Child0  | cr | dr |
  *   +--------+--------+--------+-----------+----------+----+----+
  *       4        4        4         4            4       1    1
```

### Small Atom (T=2)

```text
  *   +---+-------+-------------------+-----------+----------+----+----+
  *   | s | hash3 |        data       | RWC0dWnCn |  Child0  | cr | dr |
  *   +---+-------+-------------------+-----------+----------+----+----+
  *     1    3               8      
  *   |<---hash-->|
  *
```text
* _size_ (s) : data size, 11 bytes or less.
* _hash3_ = `(data 1st word ^ 2d word ^ 3d word) & 0xFFFFFFu`
* _size_ and _hash3_ forms a 4-bytes hash to filter false candidate when probing
* When size > 8, one retrieve 3 extra bytes of data by computing `(hash ^ 1st word ^ 2d word) & 0xFFFFFFu`

### Tag (T=3) And Blob Footprint (T=4)

```test
  *   +--------+------------+----+-----------+----------+----+----+
  *   |  hash  |   slice0   | J0 | RWC0dWnCn |  Child0  | cr | dr |
  *   +--------+------------+----+-----------+----------+----+----+
  *       4          7        1                     
  *   J = first jumper (1 byte)
```

* _jumper_
  The "jumper" is an offset multiplier to jump over unwanted content between two consecutive cells within a chain. Jumpers link segments together without mixing them with unrelated content.

  Supposing the offset constant while probing, the next segment of the string is located at `(adress + (jumper + 1) * offset) % memory_size`. However the actual prototype make the offset grows linearly as the probing process goes. So the actual formula is different.

### Intermediary Segment of a Binary String (T=5)

```text
  *   +-----------------------------------------+---+
  *   |                   data                  | J |
  *   +-----------------------------------------+---+
  *                        21                     1
```

This segment may belong to a blob footprint or a tag.

### Last Segment of a Binary String (T=6)

```text
  *   T = 6: last segment of a binary string
  *   +-----------------------------------------+---+
  *   |                   data                  | s |
  *   +-----------------------------------------+---+
  *                        21                     1
  *   s = segment size
```

* Data after the s first bytes are junk data.

### Reattachment point (T=7)

```text
   *   +--------+--------+--------------+
   *   |  from  |   to   | ... junk ... |
   *   +--------+--------+--------------+
   *       4        4          
 ```

This cell content doesn't contain any definition data. It only reattaches the "from" segment, whom jumper field reached its max value, with the segment at the "to" address.

### Children (T=8)

```text
   *   +------+------+------+------+------+---+---+
   *   |  C4  |  C3  |  C2  |  C1  |  C0  | t | d |
   *   +------+------+------+------+------+---+---+
   *       4     4      4      4       4    1   1
   *   Ci : Child or list terminator
   *        (list terminator = parent address with flag)
   *   t: terminators (1 byte with 5 terminator bits).
   *   d: directions (1 byte with 5 flags: 0 incoming/1 outgoing)
```

Note that a children cell can gather children of unrelated arrows.

* The terminator flags allow to tell if a given back-reference is the last in its list. It allows to stop searching for children when the reference counter is overflowed.
* The direction flags allows to distinguish incoming and outgoing child arrows. WARNING: If one removes this distinction, the terminator flag gets ambiguous as one doesn't know which parent arrow this back-reference relates to.

## Garbage Collector

The AS features a garbage collection process which spares rooted arrows and all their "ancestors" and gets back resources used by unrooted and unreferred arrows.

When an arrow is not rooted and has no rooted descendants, one may recycle the cells used to store its saved definition.

The GC proceeds to-be-deleted arrows in an incremental way. Its job is dispatched on many transactions to avoid CPU overhead. Since arrows are not deleted immediately, they may be saved from deletion by being rooted or linked back. An additional check of deletion criteria is performed just before the actual deletion.

_note_: In the actual code, the GC is never delayed. It is performed between every micro-transaction and cleans all reclaimable space.

## Improvements in study

Additional back-references should also be stored for quick retrieval of rooted structured arrows. For example, they may be used to quickly fetch contextualised arrows like `(C0 ->  (C1 -> (... -> (Cn -> * ))))` (all the arrows in the nested hierarchy of context `{C0:{C1:...{Cn:{*}}...}}`).

A proposal is to index every arrow by its flatten definition, or by some parts of its flatten definition like:

* the first-half of its flatten definition and the second-half
* all odd-indiced atoms, and all even-indiced atoms,
* the last atom, and the last but one atom.
* etc.
