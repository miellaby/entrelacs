# Introduction #

As explained herein,
  * the OpenAddressing strategy is a way to manage both hash collisions and data chains dispatching among the ArrowsSpace.
  * This strategy is improved by two additions:
    * Double Hashing
    * Coalesced Hashing

# Collisions #

When storing an arrow into the ArrowsSpace, one first computes an hash code noted _h_.

_h_ returns the storage cell which is supposed to receive the arrow definition. However, this cell might be already used by an other arrow.

In such a case, one makes use of the "open addressing" way to deal with collisions. It consists in putting conflicting arrows among neighbor cells.

This set of neighbor cells is called an "open block" herein. They are not necessary nearby. There may be a big offset between two successive locations within an "open block".

As a matter of fact, this offset is not necessarily constant. It may be computed by an hash function as well so to be different in most cases. This improvement prevents resonance phenomena which could lead to repeated collisions. It is called DoubleHashing.

# Data chains #

When storing a big amount of data into the ArrowsSpace, one makes use of the OpenAddressing and DoubleHashing methods to dispatch data among many cells as well.

# Induced constraints #

The whole "open block" needs to be scanned (_probed_) when one looks for an arrow copy at a given open address.

When one removes an arrow from the space, one must put a stuffing value into freed cells. This stuffing value prevents the open block to be split in several pieces and existing arrows to be missed when searching latter.

# CoalescedHashing #

The stuffing values method can't be used when one removes all cells used by a big data chain like Tags and Blogs. It would lead to quickly fill up the whole space with stuffing values.

That's why the ArrowsSpace operates the CoalescedHashing method within the DoubleHashing method.

The idea consists in linking cells used by the same data chain within an open block. Those links allow real cells freeing in the open block without losing the continuity of still used cells.

Rather that storing a fully qualified address within the storage space, one only reserves few bits on this purpose. These bits form an offset factor to be multiplied to the actual offset factor when fetching the open block content.