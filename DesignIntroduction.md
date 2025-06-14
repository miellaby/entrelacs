# Entrelacs System Architecture

Please find herein an overview of an [Entrelacs System](EntrelacsSystem.md) architecture.

## Arrow Space

![Arrow Space](pictures/mem0.png)

A major component of such a system is the [Arrow Space](ArrowsSpace.md).

It's simply a device to store arrows.

## Primary Machine and Language

The system also provides a default evaluation machine, namely the [Entrelacs Abstract Machine](EntrelacsAbstractMachine.md), that evaluates programs made in the [Entrelacs Language](EntrelacsLanguage.md).

Naturally, both programs and machine states are made out of arrows.

## System Boundaries

The currently developed prototype stands as a [software server](EntrelacsServer.md) on top of a GNU/Linux hosting system.

This server publishes a REST/HTTP API.

It is also planned to allow injecting and executing native code within the system, leveraging an internal C-like API.

See the [Entrelacs Prototype Architecture](EntrelacsPrototypeArchitecture.md) for details.

## Other Questions?

Read the [FAQ](FAQ.md).
