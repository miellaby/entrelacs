# Arrow Textual Representation

## Canonical string representation

One may use a Polish notation to flatten down the nested pairs of an arrow definition so to remove parenthesis.
For example, the arrow `((a → ( b → c )) → d)` is flattened as `→ → a → b c d`.

By using slash (/) as → and plus (+) as the atom separator, one may serialize the arrow definition into a string.
Atoms may also contain %-encoded byte such as the reserved "/" and "+" characters.

### A few examples

* `hello` is the UTF-8 encoded 5 bytes length binary string "hello"
* `/a+b` is `a → b`, that is the arrow from "a" to "b" ("a" is the _queue_, b is the _head_)
* `/c/a+b` is `c → (a → b)`, that is the arrow linking "c" to the arrow a→b
* `//a+b+c` is `(a → b) →  c`, that is the arrow linking the arrow a→b to "c"
* `/a/b+c` is `a → (b →  c)`, that is the arrow linking "a" to the arrow linking b→c
* `/a//b+c+d` is `a → ((b → c) → d)`
* `//a/b+c+d` is `(a → (b → c)) → d`
* and so on.

## Extended syntax with ambiguity solving

Ambiguous expressions are solved by following the rules bellow.

* _empty_: The empty string is a valid atom (empty atom).
  * `/a+` is `a → ''`
  * `/+a` is `'' → a`
  * `/+` is `'' → ''`

* _extra +_: Additional +-introduced atoms complete the current slash expression by linking it with the following atoms
  * `/a/b+c` is a canonical string corresponding to `a → (b → c)`
  * `/a/b+c+d` is an ambiguous string with an additional atom. It is parsed as: `a → ((b → c) → d)`
  * `/a/b+c+d+e` is parsed as: `a → (((b → c) → d) → e)`, hote how the rule tends to form a left-leaning tree of atoms.
  
* _extra pair_ : additional slash expressions also complete the current slash expression by linking it with the following expression.
  * `` = the empty string (empty rule)
  * `/a+b/c+d` is `(a → b) → (c → d)`
  * `/a/b+c/d+e` is `a → ((b → c) → (d → e))`
  * `/a+b/c+d/e+f` is `((a → b) → (c → d)) → (e → f)`
  * `/a/b/c+d` is `a → (b → (c → d))`
  * _extra + and extra_pair_ rules combined examples
    * `/a+b+c/d+e` is `((a → b) → c) → (d → e)`
    * `/a+b+c/d+e/f+g` is `((a → b) → c) → ((d → e) → (f → g))`
  
* _zero_: An empty slash with no atom is defined as Ouroboros (noted 0 herafter).
  * `/` is `0`
  * `/a/` is `a → 0`
  * `/a+b/` is `(a → b) → 0`
  * `/a/b/` is `a → (b → 0)`

* _uncomplete_: A slash expression with only one parent arrow is an arrow from the arrow to the empty string.
  * `/a` is `a → ''`
  * `//a+b` is `(a → b) → ''`
  * `/a+b/c` is `(a → b) → (c → '')`
  * `/a/b/c` is `a → (b → (c → ''))`
  * `/a/b/c/d` is `a → (b → (c → (d → '')))`
  * `/a/b+c/d` is `a → ((b → c) → (d → ''))`
  * `/a+b//c+d` is `(a → b) → ((c → d) → '')`

### Combined Examples

* `+` = `/+` = `('' → '')` (empty rule & extra + rule & empty rule)
* `++` = `'' → ('' → '')` (empty & extra + & empty & extra)
* `//a` = `(a → '') → ''` (uncomplete rule twice)
* `//` is `0 → ''` (the second / is empty, it's the tail of the first / which has no head)
* `///` = `(0 → '') → ''` (zero rule & uncomplete rule twice)
* `a/` = `a → 0`  (extra empty pair)
* `/a/`= `a → 0` (extra empty pair)
* `/a+b//` = `(a → b) → (0 → '')` (extra pair, zero, uncomplete pair)
* `/a//` = `a → (0 → '')`
* `+a` = `'' → a` (empty & extra + rule)
* `a+` = `a → ''` (extra + & empty)
* `a++` = `(a → '') → ''` (extra + & empty & extra + & empty)
* `/a+/` = `(a → '') → 0` (zero extra pair)
* `'+/a+b'` = `'' → (a → b)` (empty & extra atom)
* `'a+b/'` = `(a → b) → 0` (extra zero pair)
* `'a+b+c'` = `(a → b) → c` (extra & extra atom)
* `'a+b/c'` = `'//a+b/c+'` (extra & extra uncomplete pair)
* `'a/b+c'` = `'/a/b+c'` (extra pair)
* `'a+/b+c'` = `'//a+/b+c'` (extra pair)
* `'a/b'` = `'/a/b'` = `'/a/b+'` = `a → (b → '')` (extra & uncomplete)
* `'a/b/c/'` = `'/a/b/c/'` = `a → (b → (c → 0))` (extra & extra & uncomplete)

### File pathes as a subset of textual arrows

The textual representation of arrows is roughly compatible with file or URI _pathes_, especially by using '.' instead of '+'.

* `/some/path/file.ext` = `(some → (path → (file → ext)))`
* `/path/to/dir/` = `(path → (to → (dir → 0)))`
* `/some/path/to/file.txt.gz` = `(some → (path → (to → ((file → txt) → gz)))`

Please note `file.txt` and `/file.txt` are the same arrow. Additional conventions may be needed to distinguish absolute paths and relative pathes

## Blobs are identifiable by their footprint

In the notation above, blobs may be identified either by their full content or by their hexademical-encoded footprint introduced with a '$' character.
