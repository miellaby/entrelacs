# This page needs house cleaning since the last migration to a new data model. #

This page describes the ArrowsSpace design as it takes place in the current [Entrelacs System prototype](EntrelacsPrototypeArchitecture.md).

## Architecture overview ##

### A thread safe library ###

> The bottom software layer consists in a thread-safe C library (EntrelacsLibrary) which provides:

  * operators to compose, root, and browse arrow structures in a single persistence file of the hosting system.
  * a **very** basic functional language interpretor. It runs arrows based expressions. Its evaluation state is itself an arrow.

### An HTTP server ###

> The [Entrelacs server](EntrelacsServer.md) on top of this library allows clients to simultaneously and remotely access the arrows space via the HTTP protocol.

### And various clients ###

  * the artsy Entrelacs web terminal may connect itself to the HTTP server.
  * The Entrelacs FUSE file system directly accesses to the persistence  file by linking the AS library.
  * The REPL client also directly accesses to the persistence file.

## Complex objects support ##

The prototype also allows to handle several types of complex objects in addition with regular arrows. That is:
  * _smalls_
> > A _small_ is a piece of raw data which is small enough to fit in an arrow storage cell.
  * _tags_
> > A _tag_ is a relatively **short** binary string.
  * _blobs_
> > A _blob_ is a very long binary object whose body is stored outside the persistence file. One only stores its unique cryptographic hashed signature into the arrows space in order to distinguish singletons.
  * _tuples_ note: postponed
> > A tuple is an ordered set of _n_ arrows (_n_ >= 1).

### Reminder ###

Despite _blobs_, _tags_ and _smalls_ being raw binary objects, they are still handled like their equivalent arrow constructs, namely _entrelacs_ (discrete reentrant structures of arrows). Tuples are also managed as they were arrows structures. So we indifferently use the "arrow" term herein to designate these objects as well.

## Mass storage usage ##

The persistence file is operated as a bank of storage units called _cells_.

One cell :
  * is identified and located by a cell address,
  * may contain a pair of addresses and/or a couple of other things.

Basically, as a regular arrow is a pair of arrows, it is stored as a pair of addresses within a single cell.

## Connectivity ##

For each arrow _a_, one also stores 2 back-references attached to its _ends_ -head and tail- definition, that is:
  * one puts _a_ address into a cell "next to" its _tail_ definition,
  * one puts _a_ address into a cell "next to" to its _head_ definition.

These back-references form an index to browse all children arrows of a given arrow.

See what "next to" expression means hereafter.

## Open Addressing ##

One relays on OpenAddressing to manage conflicts within the storage space.
  * When an object definition can't be stored at its expected location because of some already present content, one repeatedly shifts the storage location by a variable but predictable offset until one finds a free cell.
  * When one looks for something, one starts a search from the object expected location, and one probes for the same sequence of cells until one reaches the object (hit!) or an empty cell (miss!).

## Little Thumbling's algorithm ##

An issue with Open Addressing is that one can't empty out a cell without breaking one or several sequences of busy cells up to relevant content.

The current prototype chose to manage a dedicated "more" counter attached to every cell. One increments this counter for every cell that one's visited from the default location up to the actual location of an added object. These counters are comparably decremented upon object removal.

Thanks to "more" counters, one knows when one must keep on probing ("more" counter greater than zero).

## Complex object (binary strings) storage ##

Blob signatures and tags are also stored into cells like regular arrows. But such an object definition can't fit into a single cell. It is taken into slices and dispatched into several cells separated by a predictable offset.

One also stores a checksum previous to the data string to accelerate singleton identification.

## Chained cells and _jumpers_ ##

Cells used to store data slices needed to be chained together so to avoid mixing them up with unrelated cells. They are called "chained cells".

Chained cells are also used to store "tuple" definitions and children lists.

Chained cells are chained together by "jumper"s which allows to _jump_ over unwanted data from one cell up to the next in the chain.

One doesn't need to store a fully qualified address to identify the next cell in a chain. One only needs to keep a short count of shifts until the next relevant cell. So a "jumper" is simply an offset multiplier stored as a short-sized counter in every chained cell but the last.

## Reattachment cells ##

A _jumper_ as introduced above is a short-sized data. Its max value is very low, but so is the probability of many repeated **consecutive** collisions with existing cells (see double hashing explanation bellow).

If however the number of shifts up to the next cell in a chain is so high that the _jumper_ reaches its maximal value, one looks for the continuation of the chain by probing a special _reattachment_ cell, that is a cell containing enough data to unambiguously identify the remaining of the interrupted chain.

## Overview of stored data ##

For each arrow, one needs to store:
  * its definition, that is
    * two arrow references for a pair
    * a checksum and a binary string for a tag
    * a checksum and a binary string for a blob cryptographic signature.
  * its root flag (one mutable bit)
  * its connectivity data, that is:
    * a list of back-references to incoming and outgoing children

## Cell categories ##

As different kinds of data may be found in a cell, one needs to identify a cell content by a few bit length value named "category" hereafter.
  * A: Arrow
  * I: Small (aka Integer)
  * S: Tag
  * B: Blob
  * T: Tuple
  * C: Chained cell (but the last one)
  * L: Last cell in a chain
  * X: Reattachment cell

## Hashing definitions ##

One makes use of an hash function to compute an arrow's _open address_ out of its definition.

The hash function is chosen after the type of arrow:
  * For a _blob_, one uses a cryptographic unambiguous hash function (SHA).
  * For a _tag_, one uses a simpler checksum. When comparing an assimilated tag with a potential copy, one must check every char of the string.
  * For a regular arrow, one hashes the tail and head's respective hash codes. The hash function might be commutative (ToBeConfirmed) so that one may deduce an arrow hash code by combining its furthest ancestor hash codes rather than fetching every intermediate parent arrows.

The overall hash based location function _h_ consequently verifies :
  * _h_(Pair) = some\_math\_of(h(tail(Pair)), h(head(Pair)))
  * _h_(Tag) = checksum(Tag)
  * _h_(Blob) = footprints(Blob) (cryptographic hash function).

## Double Hashing to minimize probing duration ##

When storing a new arrow and upon a location conflict, One could decide to look for a free cell immediately after a conflicting cell. But this naive approach would lead to _data cluttering_.

A better approach consists in shifting from the default used location  by an offset which is different for each stored content in order to minimize cluttering. In other words, the offset is also computed by an hash function. This particular strategy is called DoubleHashing.

When putting an arrow into the storage space, one computes a big hash code _h_ as described above, then one truncates the hash code modulo 2 twin _prim numbers_ _p1_ and _p2_.
  * H % p1 returns the open address,
  * H % p2 returns the probing offset.

_2012 note: the actual prototype also features quadratic probing. The offset grows as probing goes._

## Double Hashing to move apart chained cells ##

One could decide to reserve and fill up a set of consecutive cells to store several slices of a same arrow definition (blob, tag...). But this naive approach would lead to _data cluttering_.

A better approach consists in separating data slices by an offset which is different for each stored content in order to minimize cluttering even more. This strategy is a form of DoubleHashing strategy.

One uses different offset functions depending on whether one chains cells for children dispatching to avoid cluttering even more.

Some misc candidate offsets:
  * offset between children cells = neg(H) % p2
> > where H is the definition hashcode introduced above.
  * offset between string slices = f(base address, checksum) % p2

## Cells usage summary ##

### Cell structure per category ###
This is a 8 bytes cell proposal.

#### Definition starts ####

  * Arrows and _smalls_ definitions
```
        +---<category>
        |
 <more> A <--tail@------> <--head@------> <r><child0> : an arrow
 <more> I <--data-----------------------> <r><child0> : a small
```

  * Complex objects definition starts
```
 <more> S <--checksum------> <--jumper--> <r> <child0> : a tag
 <more> B <--checksum------> <--jumper--> <r> <child0> : a blob
 <more> T <--checksum------> <--jumper--> <r> <child0> : a tuple
```

#### Cell chains ####
  * Cell chain to list the ends of a tuple
```
 <more> C <--end@-------> <--end@-------> <--jumper--> : 2 ends of a tuple
 <more> L <--end@-------> <--end@-------> <size = 0/1> : Last ends of a tuple
                                                           (size = 0 ==> 1 end only)
```

  * Cell chain to list the slices of a tag (or a blob signature)
```
 <more> C <-----slice-------------------> <--jumper--> : data slice of a blob/tag
 <more> L <-----slice-----> <--nothing--> <--size----> : last data slice of blob/tag
                                                          (last slice size stored in bytes)
```

  * Cell chain to store the children list of an arrow
```
 <more> C <--child@-----> <--child@-----> <--jumper--> : 2 children references
 <more> L <--child@-----> <--child@-----> <--unused--> : last 2 children references

Note: reference may be null if cell is half-full.
```

  * Jumper overloading management
    * For a children list
```
 <more> C <--child@-----> <--next@-----> < MAX-JUMP > : Cell chain with overloaded jumper.
     One address directly refers to the next cell in the chain.
```
    * For other cell chains
```
 <more> C       ...                      < MAX-JUMP > : Chained cell with overloaded jumper
 <more> X <--previous@--> <--next@-----> <--unused--> : Reattachment cell
```

### Fields size ###

The prototype is based upon a 8 bytes cell size.
  * _more_ counter : 5 bits
  * _category_ : 3 bits
  * _cell address_ (_tail@_, _head@_, _end@_, _previous@_, _next@_,  _child@_) : 3 bytes
  * _jumper_ : 1 byte
  * _child0_ : 7 bits
  * _root flag (r)_ : 1 bit
  * _size_ : 1 byte

### Fields usage ###

  * _more_
> > The "more" counter of the Little Thumbling's algorithm introduced above.

  * _category_
> > The category indicates the inner structure of a cell and give information about its content. Note C/L chained cells may store different kinds of data (arrow children, binary slices, tuple ends).

  * _jumper_
> > The "jumper" is an offset multiplier to jump over unwanted content between two consecutive cells within a chain. Jumpers link together C, L, S, B or T cells. The next cell in the sequence is located at `@cellR + ( jump + 1) * offsetNext`.

_2012 note: formula above doesn't take into consideration quadratic probing in actual prototype_

  * _child0_
> > Every cell containing the beginning of an arrow/object definition is also the first cell of a "children cells chain", a list of cells separated by a dedicated "children offset". "child0" is is an offset multiplier (like _jumpers_) to jump over unrelated content up to the first real children cell. The beginning of the children list of an arrow at _@parent_ is located at `@parent + children * offsetChildren(parent)`. Note that "children" to zero means the arrow has no child.

  * _root bit_
> > The root flag as a mutable bit

  * _size_
> > L cells may be partially full. The _size_ field allows to ignore a stuffing value.

## Arrow typical footprint ##

Finally, here is the ideal (no conflict) footprint of an arrow _a_ (its ID and also its address). _a_ goes out a _tail_ arrow and comes into an _head_ arrow (two other addresses). _a_ is also referred by several incoming or outgoing _children_.

| To be done |
|:-----------|

## RAM Cache ##

When needed, arrows climb up to a level 1 (RAM) memory device. This memory is used as a fine-grained cache of level 0 cells.
See ArrowsCache for further details.

## Garbage Collector ##

The AS features a garbage collection process which spares rooted arrows and all their "ancestors" and gets back resources used by no more rooted nor referred arrows.

When both an arrow is non-rooted and has no child, one can recycle its storage resources.

The GC proceeds to-be-deleted arrows in an incremental way. Its job is dispatched on many transactions to avoid CPU overhead. Since arrows are not deleted immediately, they may be saved from deletion by being rooted or linked back. An additional check of deletion criteria is performed just before the actual deletion.

_2012 note: not incremental yet. GC is performed at the beginning of each commit._
## Summary of hash functions ##

P0 and P1 are twin prim numbers just above 2 ^ 24 (address space size).

Here are the hash functions currently used by the prototype.
  * regular arrow hash = h(a) =  f(tail(a), head(a)) = ((head(a) << 24) ^ tail(a))
  * blob hash = h(blob) = SHA-2(blob)
  * tag hash = h(tag= = simple string checksum, like in http://www.cse.yorku.ca/~oz/hash.html
  * arrow location hash = h(a) % P0
  * arrow drone hash = h(a) % P1
  * chainOffset hash = h(base-cell-content, base-cell-address);
  * children hash = neg(chainOffset)

_To be confirmed. See [actual code](http://code.google.com/p/entrelacs/source/browse/trunk/space.c)._

#### Still studied alternatives/improvements to this proposal ####
  * the arrow key hash function might be commutative on structure of pairs.
  * About arrows back-reference (connectivity data).
    * No real need to chain them as wrong back-reference are tolerable.
    * They might be detached from arrow definition and be dispatched to some hash-based location. Thus it could merge connectivity data of several arrow, or at the contrary, it could split data into subsets.
    * Typically, it would be better to directly fetch a subset of back-references for a given context, like fetching all outgoing arrows from A (context) whose head is B (parent target), that is the A->(B->**) matching pattern. More generally, it would be interesting to quickly fetch arrows by path patterns like (A->(B->(...->(E->**))))