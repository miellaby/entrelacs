This enhancement to OpenAddressing consists in linking data cells within the ArrowsSpace when storing a big chain of data among many cells.

Links needn't to be fully defined addresses as each link only needs to identify one of a few of the next cells within the probing sequence. So such a link is only a small offset factor which allows to jump over freed or unrelated cells within an open block in order to directly get successive pieces of a coherent chain of data.

This offset factor only needs for a few bits within a storage cell as repeated collisions for successive cells within an open block should be very rare thanks to the DoubleHashing enhancement to OpenAddressing.

If this factor limit is reached. One must degrade the search method to a traditional DoubleHashing method until a free or stuffed cell is found.