The ArrowsCache is caching cells from the arrows bank into RAM. It's a major part of the ArrowsSpace.

The ArrowsCache consists in an array of cell-containers (also called level 1 cells).

Each level 1 cell stores:
  * the whole content of a corresponding level 0 cell,
  * the level 0 cell address,
  * a modified state bit

On cache miss, the system must copy a level 0 cell into a level 1 cell. The level 1 address simply corresponds to the level 0 address modulo the cache size.

If the level 1 cell already contains a **modified** cell, then one faces a problematic conflict. That's why a second very short array of cells is defined. This second array stores every modified cell which has to be replaced by an other cell in the main array. This second array is managed according to the OpenAdressing strategy.

Furthermore, all changes in level 1 cell contents are logged with rollback data (i.e. cell contents before modification).

On commit:
  * Firstly, the change log is recorded to disk.
  * Then, all these changes are applied into disk.
  * Finally, the change log is cleared from disk. The second RAM array and the modified bits of the first RAM array are cleared as well.