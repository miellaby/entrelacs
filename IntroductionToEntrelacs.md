<img src='http://entrelacs.googlecode.com/svn/trunk/doc/pictures/car.png' align='left' />
### stupid, stupid computers! ###

Have you ever found software technologies disappointing? I consider on my own they are what mechanical engineering would have been if nobody had ever invented the wheel.

What's wrong? Mainly the way computers are used to store information: the _value in a box_ principle.
<img src='http://entrelacs.googlecode.com/svn/trunk/doc/pictures/box.png' align='right' />

The  _value in a box_ principle is  what human beings are accustomed to, because it's a straightforward adaptation of this ancient technology named "writing", which also consists in putting signs to expected locations on some adequate support. That's why this principle is deeply buried in people's mind, forming a mental lock which prevents programmers and theoreticians imagining something else.

Now, everything computer-related is designed according to the _value in a box_ principle: operating systems, programming languages, data structures, and even recently created data formats like RDF.

And yet the _value in a box_ solution is terribly **limited**, simply because it can't accept "some value in some box" relation neither as a box nor as a value. That's what makes computers unable to abstract or elaborate any aspect of their knowledge. In short, that's what makes computers **stupid**.

The EntrelacsSystem proposes to get ride of this weak "value in a box" way of thinking. Starting from now, one will build knowledge representation out of one and only one kind of building blocks: _arrows_.

### arrow, what is it? ###

<img src='http://entrelacs.googlecode.com/svn/trunk/doc/pictures/arrows1.png' align='right' />
In short, an arrow is... a pair of arrows.

Coders may think of it as a structure of 2 pointers linking such structures (C), or as a _con_ of cons (Lisp), or as a record in a twice reentrant relation table (SQL), or as an URI returning a pair of URI (web), etc.

The arrow twice recursive definition is self-sufficient. One says the underlying theory features homoiconicity, meaning that, contrary to graphs studied in academic institutions, it doesn't depend on a concepts duality like _vertices vs.edges_.

That's what makes arrow structures different. Usual models don't fit them. For example, one might consider arrows from language theory point of view and describe them as a formal grammar without terminal symbols. And so?

<img src='http://entrelacs.googlecode.com/svn/trunk/doc/pictures/eve.png' align='right' />

### how can one get rid of vertices? ###

_Entrelacs_ are intricate balls of arrows linked each others. For example, the arrow whose tail and head both get back to itself forms a simple entrelacs. One can build an infinity of entrelacs out of any number of arrows.

Entrelacs are interesting reentrant structures.
  * One can't isolate smaller structures within.
  * but one can still recognize them according to their topology (i.e. how arrows are linked each others). For example, one can recognize and name "Eve" this entrelacs whose single arrow features : Eve's head = Eve's tail = Eve.

In other words, entrelacs are somewhat equivalent to terms. They behave like symbols, literals and raw data. And vice-versa: the "Hello" string is equivalent to a specific entrelacs, while "world" is an other entrelacs. All in all, everything is an arrow.

When an arrow is not part of an entrelacs, it's a link between two things, either an entrelacs or another link. Such an arrow can be linked in turn by some other arrows in a constructive manner.

### when meaning is made explicit in meta-context ###

Entrelacs (signs) and regular arrows (to link signs together) are more than enough to represent any form of knowledge.

But arrows have no default meaning. Meaning depends on the observer's point of view.

Now, let's suppose one defines a new programming language where Eve gets a particular meaning.

In a traditional system, such a sign-to-meaning relationship would be hardcoded in an interpreter or a compiler. It would consequently stay out of system scope.

But in an arrow based system, arrow-to-meaning relationship is made explicit. It takes the form of additional arrows within an upper context. In this example, the new programming language would be interpreted by an abstract machine whose behaviour would be defined as well as arrows. The meaning of these arrows could possibly be explained in turn. And so on up to a primary machine.

The primary machine is actually the only machine whose behavior description resides out of the system scope. It could even be hardware based.

### arrows as pure concepts ###

What can an human being do when he thinks about a concept like the pi number? Well, his brain can easily access all sorts of known information about it, like when and how it is used, what concepts are related to it, and so on. By digging into his memory, the brain can remember even more personal memories. For example, a mathematician would probably remember the first teacher who told him about pi.

In the same time, a regular computer stores plenty of "pi" copies -either its approximative value or its textual symbol- in various "boxes" but it can't make any reasoning about pi as a pure concept.

Comics caption:
  * _Hey computer! Can you tell me when you make use of pi and what approximations you use?_
  * bzz bzz.....!
  * _Come on, computer! I just want you to use more decimals next time._
  * bzzzzzz...

To make computers more useful, an arrows based system forbids duplicates. It ensures each mathematically definable arrow, like _Eve_ or the arrow from "hello" to "world", is mapped to at most one physical copy in the whole storage space.

The point is not so much to spare storage space. The goal is to make the computer as capable as an human being. One wants the system being able to recognize as an unique object the "hello" and "world" link every time he handles it, then being able to find all the known information about this object.

Now you may ask: "What happens when one changes an arrow copy which is involved in many contexts of knowledge and behavior?"

The response is that you can't change an arrow once it's physically put in the storage space. This is another key concept of such a system: Stored arrows are _immutable_.

Arrows structures are consequently _persistent_. It means the user interactions don't alter existing structures but creates new structures nearby. Hopefully, most of the existing material will be reused to build new structures, since arrows are never duplicated.

The idea could be compared with the "copy on write" feature of disk file system, except that one doesn't write over anything nor make a copy of anything.

### the Arrows Space: an unique, giant, reentrant hash table ###

Ensuring each arrow is stored only once might be costly when converting new data into arrows. The solution consists in considering the whole computer storage space (hard disks) as a giant content-indexed open-addressed hash table mainly containing back-references within this same table.

The so-called _arrows space_ features _orthogonal persistence_. This buzzword means that the system blurs the old computing boundaries like RAM, disks and files... The Arrows space as a whole forms a convenient and consistent work space where every agent is alive and every piece of information is saved on the fly.

### rooting or recycling knowledge ###

The whole system knowledge at a given time might be contained in a single unique arrow definition. However this theoretical approach appears not to be so handy.

The preferred solution consists in adding a "root flag" to each stored arrow. This boolean value determines whether the arrow belongs to the system's _root context_.

An incremental garbage collector algorithm works inside the Arrows Space. It spares rooted arrows and all their ancestors, but eventually recycles any storage resources. This algorithm deals with reference counters but contrary to traditional system, such a simple algorithm perfectly does the job here.

### arrows are not a language... ###

I guess at this step of this reading, you might wonder what kind of programming languages the EntrelacsSystem refers to.

The fact is that arrows underlying theory doesn't compete with Lambda Calculus and any other computation models (combinatory logic, pattern matching languages, ...).

Quite the contrary, arrows storage is the ideal complement to purely functional languages. The arrows theory is a fundamental theory of knowledge representation while Lambda Calculus and its relatives are fundamental theories of computation.

Because of the mental lock I told you about, language theoreticians have ignored the knowledge representation problem, being condescending with engineering trends like object-oriented modeling or tagsonomies. Those trends are actually naive attempts to enhance the way one stores information in a computer.

That being said and as a matter of fact, one may adapt any language of any kind: imperative or declarative, functional or object oriented, etc. Arrows are ideal constructs to embed evaluation contexts in each others. So many abstract (understand software) machines, corresponding to many languages and technologies, could be hosted simultaneously within a single EntrelacsSystem.

Creating a Domain Specific Language could actually be the best way to add a new behavior into the system. In other words, each application -even your favorite office application- might be designed as an abstract machine processing user inputs and computing some outputs according to a dedicated graphic/command language.

Such a machine could be defined on a top of a higher level abstract machine, which could be in turn defined on top of a even higher machine, and so on up to some _primary machine_.

### ... but they need for a new language ###

The root context of an arrows evaluation stack needs for a primary language evaluated by a primary machine. How could it looks like: Lambda calculus, combinatory logic, ...?

The EntrelacsSystem forms a new basis to invent a language combining both a purely functional computation model and a purely relationist storage model. Such a language could leverage on arrows to get innovative properties. To name a few:
  * machine-level reflexivity: the whole machine state could be acceded by programs as a first order object. It would include closures, continuations chains, partial evaluations, environments, etc.
  * transparent "memoization": A cache of any intermediate computed results would automatically prevent repeating twice the same computation.
  * merged source code, CST and AST constructs: Arrows based construct can replace them all.
  * persistent reflexivity: programs can easily make persistent changes into themselves (as source = CST = AST = arrows). It makes reflexivity much more powerful than in most today's systems.

### conclusion ###

In conclusion, the EntrelacsSystem is not only a new way to store information within a computer. It's a plain new computing environment.

Please contact me if you like to contribute to this project. There's a lot of work.
  * I guess one could design an arrows space prototype rapidly.
  * In parallel, one should choose or invent a convenient language as a primary abstract machine. I'm currently exploring how I could adapt a CaEK machine. If anybody finds this idea exciting, I'd be happy pass on the torch.