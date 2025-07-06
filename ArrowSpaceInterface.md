# Code


This is C++ code to illustrate how an [Arrow Space](ArrowsSpace.md) works. The actual [prototype](ArrowsSpacePrototype.md) has a C interface.

```cpp
#include <cstdint>
#include <string>
#include <functional>

class XLEnum;

class XLArrow {
public:
    using ID = uint32_t;
    using XLCallBack = std::function<ID(Arrow arrow, Arrow context)>;

    using enum XLType {
        XL_UNDEF = -1,
        XL_EVE = 0,
        XL_PAIR = 1,
        XL_ATOM = 2
    };

    static constexpr ID NIL = 0xFFFFFFFFU;
    static constexpr ID EVE = 0;

    // Construction of transient arrows
    // ------------------
    XLArrow(); // build a Eve copy
    XLarrow(XLArrow tail, XLArrow head)
    XLarrow(const XLArrow* tail, const XLArrow* head); // pair of arrows
    explicit XLarrow(const char* str); // C-string Atom
    explicit XLarrow(uint32_t size, const uint8_t* data); // Binary Atom
    explicit XLArrow(const std::string& uri); // Assimilate an URI. Returns an arrow or NIL if bad URI
    explicit XLArrow(const std::string_view uri_part); // Assimilate a piece of URI. Returns an arrow or NIL if bad URI
    // ... assignement
    XLArrow(const XLArrow&);
    XLArrow& operator=(const XLArrow&) = default;

    // Assimilation (getting singletons)
    // ------------------
    XLArrow assimilate(); // assimilate arrow by getting its corresponding singleton
    XLArrow resolve(); // assimilate arrow by getting its singleton if found otherwhise return this

    // Getters (they don't assimilate)
    // ------------------
    ArrowID getID() { return ID; }
    XLType  getType() const; // Get arrow type
    XLArrow getHead() const; // Get arrow head
    XLArrow getTail() const; // Get arrow tail
    uint32_t getChecksum() const; // Get arrow hash/checksum
    char*   getDigest(uint32_t* size_p) const; // Get arrow digest. Returns pointer to allocated memory
    char*   getUri(uint32_t* size_p) const; // Get arrow definition in URI notation. Returns pointer to allocated memory

    // Getters for Atoms (TODO Sub-class)
    // ------------------
    char*   getStr() const; // Get atomic arrow as a C string. Null terminator added. Returns allocated memory or null if pair
    uint8_t* getMem(uint32_t& size) const; // Get atomic arrow as binary data. size updated. No extra null terminator added. Returns allocated memory or null if pair
    void*   getPointer() const; // Get the C pointer of a "hook" arrow, otherwise NULL or null if not "hook"

    // Testing (resolve but don't assimilate)
    // ------------------
    bool isEve() const; // Returns true if equals Eve
    bool isKnown() { return resolve().singleton != NULL }; // Return true if known
    XLArrow isRooted() const; // Returns this if rooted, Eve otherwise
    XLArrow equal(const XLArrow& other) const; // Returns this if equals other, Eve otherwise
    XLArrow isAtom() const; // Returns this if an atom, Eve otherwise
    XLArrow isPair() const; // Returns this if a pair, Eve otherwise

    // Rooting
    // ------------------
    XLArrow root(); // Assimilate and root arrow, return singleton
    XLArrow unroot(); // Resolve and unroot arrow, return resolved arrow (this if no singleton)

    // Assimilated Arrow factories (factories of Singletons)
    // ------------------
    static XLArrow eve(); // Returns Eve
    static XLArrow pair(const XLArrow* tail, const XLArrow* head); // Assimilate a pair of arrows
    static XLArrow atom(const char* str); // Assimilate a C string
    static XLArrow atomn(uint32_t size, const uint8_t* data); // Assimilate raw data
    static XLArrow uri(const char* uriStr); // Assimilate an URI. Returns an arrow or NIL if bad URI
    static XLArrow urin(uint32_t size, const char* uriStr); // Assimilate a piece of URI. Returns an arrow or NIL if bad URI

    // Compound arrow factories
    // ------------------
    static XLArrow anonymous(); // Assimilate a randomized value to get a unique "anonymous" arrow
    static XLArrow hook(void* hook); // Assimilate a C pointer and return an arrow

    // Arrow resolving without assimilation
    // ----------------------------------
    static XLArrow pairMaybe(const XLArrow* head, const XLArrow* tail); // Return the resolved singleton for this pair otherwise a transient XLArrow
    static XLArrow atomMaybe(const char* str); // Return the resolved singleton for this string otherwise a transient XLArrow
    static XLArrow atomnMaybe(uint32_t size, const uint8_t* data);  // Return the resolved singleton for this binary string otherwise a transient XLArrow
    static XLArrow uriMaybe(const char* uriStr);  // Return the resolved singleton for this URI otherwise a transient XLArrow
    static XLArrow urinMaybe(uint32_t size, const char* uriStr);  // Return the resolved singleton for this URI part otherwise a transient XLArrow
    static XLArrow digestMaybe(const char* digest);  // Return the resolved singleton for this digest otherwise a transient XLArrow

    // Browsing methods (resolve but don't assimilate)
    // ------------------
    XLEnum* childrenOf(); // Return an enumerator of children
    void childrenOfCB(XLCallBack callback, XLArrow context); // Call back with every child

    // System Life Cycle
    // ------------------
    static int init(); // System initialization
    static void destroy(); // Release resources and locks

    // Transactions
    // ------------------
    static void begin(); // Open a transaction
    static void over(); // Release a transaction
    static void commit(); // Wait for all transactions and commit.
    static void yield(const XLArrow& state); // Wait for all transactions and perform a GC without commiting, removing all loose arrows but a "state" arrow

private:
    xl_arrow_def_t def; // the transient definition
    ID id = NIL; // the arrow id
    XLArrow* singleton = NULL; // the corresponding assimilated Arrow, NULL is not solved, this if singleton
};

class XLEnum {
public:
    XLEnum();
    ~XLEnum() = default;
    bool next(); // Iterate enumerator. Return false if over or broken, true otherwise
    XLArrow get() const; // Get current arrow from enumerator
};

```
