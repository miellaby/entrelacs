# Arrows Space

## Introduction

An _"Arrows Space"_ (AS) is a store for arrows structures involved in the [Entrelacs Paradigm](ArrowParadigm.md). It's the key component of an [Entrelacs System](DesignIntroduction.md).

Please note one may simulate an Arrows Space with [pen and paper](PenAndPaperReferenceDesign.md).

One day, we may invent a new piece of hardware working as an AS. If so, it won't necessarily consist of a bank of data buckets like all other data storage so far.

For now, we introduce the AS as a piece of software leveraging traditional hardware, that is, volatile and mass memories.

<div align='middle'><img src='pictures/mem0.png' /><br /><u>Artistic view</u><br />Arrows are basically stored as pairs of pointers</div>

## AS Requirements Summary

An AS must meet the following requirements:

- **Orthogonal persistence**  
  - The AS must leverage all persistent and volatile physical memory levels and flatten them into a single homogeneous space.  
  - Neither developer nor end users should need to worry about whether data resides in RAM or on disk; the AS abstracts this complexity.

- **De-duplication**  
  - The AS must ensure that each abstract arrow is stored at most once in the whole storage space.
  - If one stores the same arrow at two different times, the AS recognizes this and avoids duplication.

- **Full connectivity**  
  - The AS must store enough connectivity data to allow efficient retrieval of the ends and the _children_ of any arrow.
  - When queried for a abstract arrow, the AS must quickly return its corresponding representation within the system, as well as any arrows derived from it.

- **Garbage collection relative to a _root_ referential**  
  - The AS must manage the _root_ boolean property of every stored arrow. The set of _rooted_ arrows forms a unique absolute referential.  
  - The AS must preserve rooted arrows and their ancestors from deletion, while arrows that are neither rooted nor ancestors of a rooted arrow must be transparently removed from the storage.
  - If an arrow becomes unreachable from any roots, it should be quickly deleted to free up space.

- **Binary strings and other complex objects native support**  
  - The AS may allow raw data (binary strings) native storage.  
  - The AS should allow tuples (ordered sets of arrows) native storage.  
  - Such objects must be treated like their equivalent arrow-made constructs in terms of uniqueness, immutability, and connectivity.
  _Example:_ A picture may be stored in the AS like an arrow would, by ensuring its uniqueness and immutability.

## API Overview

The [ArrowSpaceInterface](ArrowSpaceInterface.md) provides a high-level abstraction for interacting with the AS. It allows developers to:

- Create and store arrows.
- Retrieve arrows by their identifiers.
- Query relationships between arrows (e.g., parent-child relationships).
- Manage the _root_ property of arrows.

## How to Reach These Goals Within Existing Hardware

### Prevent Duplicates During Arrows Assimilation

The _Arrows Space_ is populated by converting data into arrows. This conversion process is called _assimilation_. It occurs:

- When data—like user inputs—come from the outside.
- When some internal computation produces new arrows.

Each new arrow is assimilated from its farthest ancestors, that is, _entrelacs_. One first assimilates entrelacs, then entrelacs' children arrows, then their grandchildren, and so on up to the overall assimilated arrow.

To ensure each mathematically definable arrow is stored at most once, the assimilation process may operate the whole mass storage device as a giant content-indexed open-addressed reentrant hash-table.

#### Main Steps of an Arrow _Assimilation_

1. **Hashing the arrow definition** to get a default location.  
2. **Looking for the existing singleton** at the default location.  
3. **Storing the arrow definition** if it is missing.  
4. **Managing conflicts** through _open addressing_ as introduced below.

### "Hash Everything!"

The first step of the arrow assimilation process consists in computing a default location where to probe the arrow singleton and store it if missing.

- **For a regular arrow**, since it's basically a pair of ends, one computes its default location by hashing its both ends' addresses.  
- **For raw data**, the AS should hash the entire object body.  
  - For a very long binary object, a **cryptographic hash function** is used to get both a default location and a unique signature:  
    - The signature is stored next to the object body to quickly identify a singleton.  
    - A cryptographic hash also avoids _data cluttering_.  
  - For a shorter binary object, a simpler **checksum** is used:  
    - This checksum doesn't uniquely identify the singleton. One must fully compare the candidate singleton to the assimilated object.

### Solve Conflicts by Open Addressing

Once truncated modulo the total mass storage space size, a hash code defines a valid default arrow location.

In case of conflict with existing unrelated arrows, one shifts this default location by some offset. If there's still a storage conflict, one shifts the location again, and so on.

Looking for an already stored arrow consequently requires searching in several locations starting from a default one:

- If one finds the expected arrow at the considered address ==> hit!  
- If one finds an empty location ==> miss! This empty location is used to store the missing arrow.  
- If one finds something else, then one keeps on searching at a nearby location.

This approach is called [open addressing](OpenAddressing.md) when applied to hash tables. The default storage location is named an _open address_ and the search process described above is called _probing_.

### Perform Orthogonal Persistence

As a whole, the AS is a single persistent memory device merging all the memory levels of the hosting system. In other words, the AS stores arrows into a mass storage device accessed through a massive RAM-based cache. This cache operates almost all the available fast-access volatile memory resources.

## Anticipated Prototype

All the strategies introduced above are applied to the currently designed [Arrows Space Prototype](ArrowsSpacePrototype.md). See this page for more details.
