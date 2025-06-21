# Arrow Cache
The _Arrow Cache_ is in charge of caching cells from the underlying persistent memory (_disk_) into RAM. It's a major part of the [Arrow Space](ArrowsSpace.md)

The disk is subdivided into cells, called _level 0 cells_ where each cell may store an arrow or connectivity data. The ArrowsCache consists in an array of cells, also called _level 1 cells_.

Each level 1 cell is able to store:
  * a level 0 cell,
  * the address of this level 0 cell,
  * a modified state bit

On cache miss, the system copy the targeted level 0 cell into a level 1 cell. The level 1 address simply corresponds to the level 0 address modulo the cache size.

If the level 1 cell already contains a **modified** cell, then one moves this cell in a a way shorter  array of edited cells. This second array is itself an hash table handling collisions through [Open Addressing](OpenAdressing.md).

Also not that all unsaved changes are also logged in a write-ahead log (WAL) in the manner of data-base engines.

On a regular basis, changes are committed to disk:

  * Firstly, the change log is recorded to disk.
  * Then, all the changes are applied in the array of persistent cells.
  * Finally, the change log is cleared from disk. The volatile array is updated to clear _modified_ bits.
