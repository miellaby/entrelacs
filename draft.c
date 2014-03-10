/* This scrap file gathers toughs about design issues
*/


// Once one decides not to use a traditional chain hashing designed table, several exicting alternatives come along.
// First alternative: Open addressing consists in looking for an arrow at more than one physical location, starting from a so-called "open address". Double hashing improves the dispatching of physical locations of a considered open address so to prevent data "clustering".
// Second alternative: The coalesced chaining. It's a sort of chained hashing, but the chaind list is design within the hash table.
// Third and most well though approach (IMHO) is the Cuckoo hashing. It consists in lookup an item in no more than two locations (much more efficient than preceding design). But those locations are computed by two independant hash functions. Every time on inserts an item, one puts it at its frst location. If it's already fillen by an existing item, this conflicting item is moved to its alternate location, which causes a potential conflicting item to be moved in turn, and so one until there is an item whose alternate location is free (no more conflict).
// However, the most important point of the future Entrelacs design is to merge atom raw data and arrows dictionnary within the same space. Cuckoo hashing requires to move whatevers exists in a targeted location; but how to move a piece of raw data without breaking the data string?
// please note when one stumbles upon a piece of data, one can't know which data string it belongs to, and where the sequence starts. If one moves this piece of data away, one can't modify previous or next pieces to take into account this change.



// About connectivity, garbage collector, new arrow, unrooted arrow

// A freshly assimilated arrow is not connected with its parents at first.
// It stays in a "loose state" as long as it doesn't take part of any rooted arrow definition.
// All in all, such an arrow got the same status as a bound-to-be-recycled non-connected arrow.
// the connection process starts by rooting a loose arrow
// root(a) ==> if (loose(a)) { unloose(a); connect(headOf(a),a) ; connect(tailOf(a),a); }
// connect(a,child) ==>
//       - add child back-ref to h3 based a sequence.
//       - if (loose(a)) { unloose(a) ; connect(head(a),a) & connect(tail(a), a); }
// New (and initialy loose) arrows and unrooted arrows are logged into a GC FIFO
// What the GC does:
// - consumme one arrow in the log (FIFO)
// - check back-ref. If no back-ref nor root-flag.
//   - Unconnect(head(a),a) & addToGCFIFO(head(a)) 
//        & unconnect(tail(a),a) & addToGCFIFO(tail(a))
// - repeat until one goes under some thresold
//

// Prototype constraints
// Cell size: 64 bits
// Address range: 0-2^24
// Blobs are stored as files in a traditional system. A blob arrow is actually equivalent to a tag arrow but the string stored in the tag is a cryptographic hash which uniquely identifies the blob.
// A newly assimilated arrow is not connected to its parents until it's rooted or it becomes an ancestor of a newly assimilated rooted arrow.
