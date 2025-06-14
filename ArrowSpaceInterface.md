# Code

This is pseudo-code to illustrate which operators an [Arrow Space](ArrowsSpace.md) might provide as a software service. This is only a thought experiment, not actual code.

```c++

class Arrow { // Proxy. Not the "real" arrow.

   private:
      Arrow();
      FullyDefinedArrow ref; // private reference to the real arrow
      
   public:
     // setter: there is only one mutable property: the root flag
    void root(Boolean rootFlag) { AS.getInstance().root(this, rootFlag); }

     // getters
     Arrow head() { return AS.getInstance().head(this) ; };
     Arrow tail() { return AS.getInstance().tail(this) ; };
     BinaryString getRawData() { return AS.getInstance().getRawDataOf(this) ; };
     Boolean isRooted() { return AS.getInstance().isRooted(this) ; };

     // testing
     Boolean operator== (const Arrow &a) const { return this == a; };
     Boolean isEve() { return this.ref == AS.getInstance().Eve().ref; };

    ~Arrow() { AS.getInstance().release(this); }
};



class CoreArrowDefinition: public Hashable {
    union d {
       BinaryString rawData;
       struct p { // pair
           CoreArrowDefinition tail;
           CoreArrowDefinition head;
       };
    };
    
   Hash getHash() {
        if (d.rawData)
            return d.rawData.size < 40 ? checksum(d.rawData) : cryptoHash(d.rawData);
        else
            return someMix((int)d.p.tail, (int)d.p.head);
   };
};

// Core arrow definition + connectivity stuff
class ConnectedArrow: public CoreArrowDefinition {
    public:
       Boolean rootFlag;
       List<ArrowDefinition&> children;
       int  lockCounter; // moved  by proxy construction and destruction
       Boolean isLoose() { // criteria for deletion by GC
           return children.size == 0 && lockCounter == 0;
       };
}

class AS { // Arrow Space
   private:
     Dictionary<CoreArrowDefinition, ConnectedArrow> dictionary;
     List<FullyDefinedArrow> queue; // commit queue

   public:
    static ArrowSpace getInstance();
   
    // proxified arrows retrieving (factory pattern)
    Arrow arrow(Arrow, Arrow); // get a regular arrow
    Arrow Eve(); // get Eve
    Arrow arrow(BinaryString); // get the atomistish arrow for this string
    
    // lock/unlock arrow
    void    lock(Arrow, Boolean) ; // called by Arrow constructor and destructor.

    // arrows probing (won't create them if not already know)
    Arrow probe(Arrow, Arrow); // return NULL if non existent
    Arrow probe(BinaryString);  // ditto

    // root flag
    void       root(Arrow, Boolean); // setter
    Boolean isRooted(Arrow); // getter

    // destructuring
    Arrow tail(Arrow); // or return the arrow itself if atomistish
    Arrow head(Arrow); // or return the arrow itself if atomistish
    BinaryString getRawDataOf(Arrow); // or return NULL for a non-atomistish arrow
    
    // browsing
    void   mapChildren(Arrow, Functor); // map Functor to every arrow child

   // committing: recycle loose arrows (those without chidren nor proxy) and save to disk
   void commit(); 
}

```
