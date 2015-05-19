# intensional definition #

A straightforward way to store knowledge with arrows consists in intensionally defining a function from referrer to referee with links like:
```
  (color,red)
  (age,150)
  (height,3m)
```

# contextualizing #

Directly rooting statements would mix unrelated information in the same top-level context. It's better rooting arrows which refers such statement arrows by a context entity.

For example:
```
   (Bob,(age,20))
```

Those contextualizing arrows can be contextualized in including contexts as well. This abstraction process can be repeated as much as necessary, to form a context meta-hierarchy.
```
   (Jane,(Belief,(Bob,(age,20))))
```

The entity used as a context depends on the paradigm in use. Here is an overview of the possible choices for a context.

# Overview #

## Object Oriented ##
The context is the object that one knows information about. One roots arrows between the object arrow and from-property-to-value arrows.

| ![https://github.com/miellaby/entrelacs/blob/master/doc/pictures/OO.png](http://entrelacs.googlecode.com/svn/trunk/doc/pictures/OO.png) | Context = Object |
|:--------------------------------------------------------------------------------------------------------------------------------|:-----------------|

## Functional Oriented ##
The context is a function that gathers all known statements about the value of this function when applied to various objects. One consequently roots arrows between a function  arrow and from-object-to-value arrows.

| ![https://github.com/miellaby/entrelacs/blob/master/doc/pictures/FnO.png](http://entrelacs.googlecode.com/svn/trunk/doc/pictures/FnO.png) | Context = Function |
|:----------------------------------------------------------------------------------------------------------------------------------|:-------------------|

## Neuronal network ##

One may still choose to not contextualize statements. It leads to rooting different kinds of statement in the same top-level context: property->value, objet->value and value->value relationships.

Retrieving stored information consists in propagating activation states from several input arrows. The most activated output arrow forms the network's response.

| ![https://github.com/miellaby/entrelacs/blob/master/doc/pictures/NeuralO.png](http://entrelacs.googlecode.com/svn/trunk/doc/pictures/NeuralO.png)   | Neuron model |
|:--------------------------------------------------------------------------------------------------------------------------------------------|:-------------|

### Warning ###

Neuron oriented model could be the most efficient knowledge representation strategy in terms of storage usage. But the resulting network can quickly become ambiguous when non-orthogonal statements are mixed together.

![https://github.com/miellaby/entrelacs/blob/master/doc/pictures/Confusion.png](http://entrelacs.googlecode.com/svn/trunk/doc/pictures/Confusion.png)

## Tagsonomy ##

Tagsonomies work in the same way as neuronal networks.  They have the same strength and weakness.

| ![https://github.com/miellaby/entrelacs/blob/master/doc/pictures/TagAndBundle.png](http://entrelacs.googlecode.com/svn/trunk/doc/pictures/TagAndBundle.png)   | It's easy to implement a full featured tagsonomy with arrows |
|:------------------------------------------------------------------------------------------------------------------------------------------------------|:-------------------------------------------------------------|

## Meta tagsonomy / unambiguous neuronal model ##

Ambiguity can be removed from tagsonomy by tagging tag-to-tagged relationship. For example
```
   anti->(socialism->http://capitalism.org)
```
