# Open Addressing and related techniques

The [Arrow Space](ArrowsSpace.md) is implemented ag a giant _Content-Indexed Hash table_ featuring:

- _Open Addressing_ for solving conflicts.
- _Double Hashing_ when probing so to reduce data clutering.
- _Coalesced Hashing_ to store multi-slots binary strings.

# Content-Indexed Hash Table

In an content-indexed hash-table, there is no _key_ or _identifier_: the whole value is used as a key. This is the obvious solution to store _arrows_ which are mathematical constructs mapped one-to-one onto the storage.

# Solving Collisions with Open Adressing

When assimilating an arrow, one computes an hash code _h_ of its definition.

_h_ gives the default location where an arrow is supposed to be in the storage space.

However, this default location might have already been taken to store another arrow. This is called a _hash conflict_ or a _collision_.

In such a case, one makes use of the _Open Addressing_ technique. It consists in putting conflicting arrows among neighbor slots.

This set of related slots is called an _open block_ herein. They are not necessary nearby as there may be a big offset between two consecutive slots.

# Solving Data Clutering with Double Hashing

Using the same constant offset for probing slots may cause unrelated _open blocks_ to merge as their probing sequences resonate to each other.

It may lead to very long and slow probing sequences.

However the offset used to probe locations is allowed to change depending on the probed data. So it may typically be computed by another hash function. This technique is called _Double Hashing_.

# Slot chaining with Coalesced Hashing

Some data don't fit in a single slot. One extends the _Open Addressing_ and _Double Hashing_ techniques to ventilate data segments accross the whole storage.

One also uses the _Coalesced Hashing_ technique, which consists in linking together the slots used to store related segments.

# Advanced Tricks

## Hop-o'-My-Thumb's "Seven-League" boots

The _Coalesced Hashing_ link is not saved as a full slot address. It is rather stored in the form of a short offset multiplier which allows to _jump_ over unwanted data.

## Hop-o'-My-Thumb’s white pebbles

_Coalesced Hashing_ is not used to link slots of the same probing sequence. To avoid open blocks breaking on content removal, one introduces a _pebble counter_ which counts how many probing pathes go through a given slot at a given time.

## And more ...

This [presentation](https://docs.google.com/presentation/d/e/2PACX-1vT46cf0X81h-x0aSHmoIM8pL6dJdWdPwXO_Xc2pJGPBr9XOVSBijsOiCTnCSYd-dFGUbvpYVUEq2T5g/pub?start=false&loop=false&delayms=3000) introduces the Hop-o'-My-Thumb tricks in a pleasant way.

A few other tricks related with _Open Addressing_ have been employed to fit even more information in the same storage. See the [prototype page](ArrowsSpacePrototype.md) for more information.

