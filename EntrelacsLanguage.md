## Introduction ##
The [Entrelacs language](EntrelacsLanguage.md) (EL) is a programing language evaluated by the [Entrelacs machine](EntrelacsAbstractMachine.md) within an [Entrelacs System](EntrelacsSystem.md).

## A fully _homoiconic_ and _reflexive_ language ##
The EL Source code is not textual. It resides in the form of arrow constructs within the [Arrows Space](ArrowsSpace.md). To get the best benefits from this particularity, EL syntax is roughly equivalent to the _Abstract Syntax Tree_ (AST) in _Continuation Passing Style_ (CPS) of a traditional functional language. _Please note this is not yet the case in the current prototype 1/7/11_

An EL program consequently can very easily:

  * access itself at source level,
  * modify itself and other programs at source level,
  * make these changes persistent.

In addition, the evaluating machine state itself is built out of arrows and may be accessed and edited by the currently evaluated program.

In short, EL is a fully homoiconic and reflexive language. An EL based program gets the ability to access and modify itself (i.e. both  _introspection_ and _intercession_) for both its static and dynamical aspects. It makes an [Entrelacs system](EntrelacsSystem.md) the ideal platform for reflexive computing.

## Syntax ##

See EntrelacsAbstractMachine for a full definition.