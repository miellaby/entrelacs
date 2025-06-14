# The Box/Value Paradigm

## What's a Data Paradigm?

A large part of computer science consists of studying [Data Structures](http://en.wikipedia.org/wiki/Data_structure)<sup>W</sup>, that is abstract organizations of data.

However, we should also consider the concept of a **Data Paradigm** which defines how abstract data structures are mapped onto tangible physical storage media.

Adopting such a paradigm invovles selecting a **meta data structure** to store data structures across various storage mediums, such as RAM, disks, and SSDs.

## The only Data Paradigm ever used

![Box/Value Paradigm Illustration](pictures/box_paradigm.png)

So far, abstract data structures are typically stored by dividing storage space into **boxes** that hold **values**. A value in a box is often a serie of smaller boxes, resulting in nested structures. When the content of a box is not a further subdivision of space, its value may actually point another box elsewhere. Such values are commonly referred to as _pointers_, _indices_, _keys_, etc.

This approach defines the _Box/Value Paradigm_, which uses a single _meta-data structure_ to represent all forms of digital information, such as graph vertices and edges, database tables and records, program objects and pointers, and files and folders. All information is serialized as nested boxes of binary values and identifiers.

## The Box/Value Paradigm is inherited from Writing
![Writing Illustration](pictures/Writing.png)

Why do we continue to represent information this way, and why don't programmers question it?

This may be because _Speech_ and _Writing_ shape how we think.

_Writing_ organizes information into a hierarchy of nested boxes, such as sentences containing words, where each concept is eventually broken down into elements forming a sign. Representing the same concept in different contexts often requires repeating the same signs multiple times.

To reduce redundancy, we use textual elements like pronouns to refer to concepts introduced elswhere. These act like _pointers_ or _indices_ in computers. However, _Writing_ remains an inherently redundant way to store information, turning a single abstract concept, like the number 42, into multiple physical representations.

In essence, _Writing_ serves as the reference design for the Box/Value Paradigm, which is why it is so deeply ingrained in how we think.

## The Graph Theory: The Mathematical Model of the Box/Value Paradigm

![Graph Illustration](pictures/Graphe.png)

The Box/Value paradigm is also the foundation of [Graph Theory](http://en.wikipedia.org/wiki/Glossary_of_graph_theory)<sup>W</sup>

In a graph, there are two types of elements: _vertices_ and _edges_. A vertex cannot be an edge, and an edge cannot be a vertex. Similarly, in the Box/Value Paradigm, boxes act as _vertices_ and values act as _edges_. A value can point to a box, much like an edge points to a vertex, but it cannot represent the concept of "a specific value within a specific box," just as an edge cannot act as the vertex of another edge.

Technologies like [triple stores](http://en.wikipedia.org/wiki/Triple_store) attempt to improve the Box/Value representation by replacing nested boxes with editable and indexed graphs. While this addresses some flaws, it does not change the underlying paradigm. As a result, most of the issues inherent to the Box/Value Paradigm remain unresolved, regardless of the complexity of these new technologies.

To put it simply: **any information model based on a graph structure or its variants is still a manifestation of the Box/Value Paradigm.**

## Issues with the Box/Value Paradigm

### Redundancy

The Box/Value Paradigm causes unnecessary duplication of information. The same data, like the number 42, is stored repeatedly in multiple boxes, even when it represents the same concept. This duplication increases complexity and requires resource-intensive indexing and deduplication processes.

> **Illustrative Issue**: You can't ask your computer when and how it uses a specific approximation of π or a particular English word. Local search engines provide only a partial solution.

### Compartmentalization

The paradigm creates artificial boundaries that limit how information is accessed and shared. Operating systems can browse files and folders but cannot see the internal structures of application-managed data. Programs are restricted to handling only the data they are explicitly designed for.

> **Example**: A media manager can access files in specific folders but cannot retrieve media embedded in other applications, like email attachments.

### Useless Duality

The strict separation of _boxes_ and _values_ enforces a rigid structure that limits flexibility. This divide extends to the separation of programs (code) and data, where users can only customize a program if developers explicitly expose parts of its behavior as configurable data. As a result, users are often restricted to predefined workflows and cannot easily adapt software to meet their specific needs.

> **Illustrative Issue**: A user working with an editor cannot format text in a specific way, unless the developer has provided enough parameterization. Without such mechanisms, the program's behavior is locked in its code, inaccessible to the user.

### Rigidity

The paradigm's structure makes information inflexible and difficult to modify.

Changing information often requires resizing boxes and shifting data, leading to complex and costly serialization and deserialization processes.

> **Illustrative Issue**: Users cannot modify their data beyond what application developers anticipated, even with access to open-source code.

### Opacity

The meaning of data is tied to its location within nested boxes, making it difficult to interpret without context.

> **Illustrative Issue**: Data buried in user files cannot be accessed without specialized programs. Extracting information from raw data often relies on forensic techniques out of reach of normal users.

### Broken information retrieval

The paradigm supports only one-directional browsing, from higher-level boxes to lower-level values, making reverse lookups difficult.

> **Illustrative Issue**: File systems often lack simple ways to identify which directories link to a specific file. The operating system don't even know which programs make use of a file, as programs refer to files by pathes buried in the code.

### No Meta Linking

The paradigm can't simply represent a relationship like _"such a value in such a box"_, or _"such a box in such a box"_. It conseqently struggles to handle higher-level abstractions, such as relationships between values and their contexts.

> **Illustrative Issue**: Systems cannot easily store metadata about metadata, such as tracking changes to access policies over time.

## Weak and Fragile

Links between boxes are easily broken when data is modified, leading to inconsistencies.

> **Illustrative Issue**: Media managers lose track of files when they are renamed or moved, breaking the links in their databases.

### Absolutism

The paradigm enforces rigid hierarchies and lacks support for alternative interpretations of data.

> **Illustrative Issue**: Computers rely on hardcoded behaviors because the paradigm cannot store multiple interpretations of the same information.
