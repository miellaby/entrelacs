# Arrow Textual Representation

## Canonical string representation

One uses a Polish notation to write down an arrow by flattening nested pair definitions without using parentheses. So the arrow `((a → ( b → c )) → d)` is written as `→ → a → b c d`.

By using slash as → and dot as the atom separator, one serializes the arrow definition as a string of regular characters.

* `hello` is the UTF-8 encoded 5 bytes length binary string "hello"
* `/a.b` is `a → b`, that is the arrow from "a" to "b" ("a" is the _queue_, b is the _head_)
* `/c/a.b` is `c → (a → b)`, that is the arrow linking "c" to the arrow a→b
* `//a.b.c` is `(a → b) →  c`, that is the arrow linking the arrow a→b to "c"
* `/a/b.c` is `a → (b →  c)`, that is the arrow linking "a" to the arrow linking b→c
* `/a//b.c.d` is `a → ((b → c) → d)`
* `//a/b.c.d` is `(a → (b → c)) → d`
* and so on.

## Tolerant parser

The syntax is extended with additional rules to parse uncomplete expressions.

### Rules

* %-expression: %-encoded byte may be included in atom definition, such as the "/" and "." characters.

* _empty_: The empty string is a valid atom (empty atom).
  * `/a.` is `a → ''`
  * `/.a` is `'' → a`
  * `/.` is `'' → ''`

* _extra dot_: Additional dot-introduced atom completes the current slash expression by linking it to the following atom, so to form a left leaning tree.
  * `/a.b` is `a → b`
  * `/a.b.c` is `(a → b) → c`
  * `/a.b.c.d` is `((a → b) → c) → d`
  * `/a/b.c.d` is `a → ((b → c) → d)`

* _extra pair_ : An additional slash also completes the last slash expression by linking it to the following expression.
  * `a/b.c` is `a → (b → c)`
  * `/a.b/c.d` is `(a → b) → (c → d)`
  * `/a/b.c/d.e` is `a → ((b → c) → (d → e))`
  * `/a.b/c.d/e.f` is `((a → b) → (c → d)) → (e → f)`
  * `/a/b/c.d` is `a → (b → (c → d))`
  * `/a.b.c/d.e` is `((a → b) → c) → (d → e)`
  * `/a.b.c/d.e/f.g` is `((a → b) → c) → ((d → e) → (f → g))`

* _zero_: An empty slash with no atom is defined as Ouroboros (noted 0 herafter).
  * `/` is `0`
  * `/a/` is `a → 0`
  * `/a.b/` is `(a → b) → 0`
  * `/a/b/` is `a → (b → 0))`

* _uncomplete_: A slash expression with only one parent arrow is an arrow from the arrow to the empty string.
  * `//a.b` is `(a → b) → ''`
  * `/a/b/c/d` is `a → (b → (c → (d → )))`
  * `/a/b.c/d` is `a → ((b → c) → (d → ''))`
  * `//` is `0 → ''` (the second / is empty, it's the tail of the first / which has no head)
  * `/a//` is `a → (0 → .)`
 
### Examples

* `/a/b/c` is `a → (b → (c → ''))`
* `/a.b//` is `((a → b) → 0) → 0`
* `/a.b/c` is `(a → b) → (c → '')`
* `/a.b//c.d` is `(a → b) → ((c → d) → '')`


### Examples

* `''` = the empty string (empty rule)
* `'.'` = `'/.'` = `('' → '')` (empty rule + extra dot rule + empty rule)
* `'..'` = `('' → ('' → ''))` (empty + extra dot + empty + extra)
* `'//a'` = `((a → '') → '')` (uncomplete rule twice)
* `'///'` = `((0 → '') → ''` (zero rule + uncomplete rule twice)
* `'/a'` = `'/a.'` = `(a → '')` (uncomplete rule)
* `'a/'` = `(a → 0)`  (extra empty pair)
* `'/a/'`= `(a → 0)` (extra empty pair)
* `'.a'` = `('' → a)` (empty + extra dot rule)
* `'a.'` = `(a → '')` (extra dot + empty)
* `'a..'` = `((a → '') → '')` (extra dot + empty + extra dot + empty)
* `'/a./'` = `'/a..'` = `((a → '') → 0)` (zero extra pair)
* `'a.b'` = `(a → b)` (extra)
* `'./a.b'` = `('' → (a → b))` (empty + extra)
* `'a.b/'` = `(a → b) → 0` (extra zero pair)
* `'a.b.c'` = `'//a.b.c'` = `((a → b) → c)` (extra + extra)
* `'a.b/c'` = `'//a.b/c.'` (extra + extra + uncomplete)
* `'a/b.c'` = `'/a/b.c'` (extra)
* `'a./b.c'` = `'//a./b.c'` (extra)
* `'/a.b/c.d'` = `'//a.b/c.d'` (extra)
* `'/a'` = `(a → '')` (uncomplete rule)
* `'a/b'` = `'/a/b'` = `'/a/b.'` = `(a → (b → ''))` (extra + uncomplete)
* `'a/b/c'` = `'/a/b/c'` `'/a/b/c.'` = `(a → (b → (c → '')))` (extra + extra + uncomplete)
* `'a/b/c/'` = `'/a/b/c/'` `'/a/b/c.'` = `(a → (b → (c → '')))` (extra + extra + uncomplete)

Note how:

    - trailing "/" are ignored,
    - extra leading "/" create a left-leaning structure involving empty string atoms, 
    - additional dots create right-leaning tree-structures

### File pathes as a subset of textual arrows

The textual representation of arrows is roughly compatible with file or URI _pathes_.

* `/some/path/to` = `(some → (path → to))`
* `/some/path/to/` = `(some → (path → (to → 0))`
* `/some/path/to/file.txt.gz` = `(some → (path → (to → ((file → txt) → gz)))`

But please note:

* `file.txt` and `/file.txt` are the same arrow.
* `file.txt`, `./file.txt` and `.file.txt` are 3 distinct arrows,
* `/file.txt` and `//file.txt` are distinct,
* `file.txt` and `file.txt/` are distinct.

Additional conventions are needed to distinguish absolute pathes, relative pathes, directory pathes, file pathes.
