# flow

A data-flow library and environment in C++.

## What?

I'm using the phrase "data-flow" in terms of machines, programming, and systems.
In data-flow systems, instead of instructions or control moving through them
(like computers are typically thought of as doing),
the focus is on data moving through them.

See the references section below to find out more about data-flow in general.

At present, the project uses POSIX descriptors and signals to implement the flow of data,
and applications having well known descriptors and signals to implement underlying leaf execution units.
As abstractions, these conveniently fit the data flow model and when instantiated execution is _asynchronous_.
See the [system.hpp](library/include/flow/system.hpp) header file for specifics on how systems can be designed in C++.

## Why?

Data-flow programming and architecture has been an interest of mine for years.
Additionally, it has benefits that are more attractive than ever, like:
- Lending itself better to modern computing resources.
- Facilitating a more organic style of modelling natural processes.

## Status

[![linux](https://github.com/louis-langholtz/flow/actions/workflows/linux.yml/badge.svg)](https://github.com/louis-langholtz/flow/actions/workflows/linux.yml)

This is just a work in progress at the moment.

## Requirements

- Compiler supporting C++20.
- POSIX-compliant operating system (like Linux or macOS 10.5+).

## References

- Devopedia's [Dataflow Programming](https://devopedia.org/dataflow-programming) page.
- Wikipedia's [Dataflow programming](https://en.wikipedia.org/wiki/Dataflow_programming) page.
- Wikipedia's [POSIX](https://en.wikipedia.org/wiki/POSIX) page.
- Wikipedia's [Assembly theory](https://en.m.wikipedia.org/wiki/Assembly_theory).
