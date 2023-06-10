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

At a conceptual design level, there are three types of things to think about: ports, connections, and systems.
Ports define data inputs or outputs. Connections define the binding together of output ports with input ports.
Systems meanwhile encapsulate implementations taking the data from its input ports,
processing that data, and then outputting resulting data to its output ports.
These implementations can be executable programs,
or recursively definable containers of subsystems connected via connections to each other's ports.
When systems are run, or _instantiated_, they're transformed into: instances, and channels.
Instances and channels exist until the instances exit.

## Why?

Data-flow programming and architecture has been an interest of mine for years.
Additionally, it has benefits that are more attractive than ever, like:
- Lending itself better to modern computing resources.
- Facilitating a more organic style of modelling natural processes.

## How?

### Build Library And Command Line Shell

Assuming:
- You want to use the project's name of `flow` as the directory name under which to store the project.
- You want to build the project in a separate directory named `flow-build`.

From a terminal that's in the directory you want `flow` and `flow-build` to appear in, run the following:
1. `git clone https://github.com/louis-langholtz/flow.git`
1. `cmake -S flow -B flow-build -DFLOW_BUILD_SHELL=ON`
1. `cmake --build flow-build`

### Startup Command Line Shell And Get Help

From a terminal that's in the directory containing `flow-build`, run:
1. `./flow-build/bin/shell`
1. `help`

This shows the various builtin flow commands that the shell knows about and that you can run.

### Run Hello World And Exit

From within the command line shell, define a system for echoing "hello world", instantiate it in the foreground, and then exit:
1. `systems set echo_hello_world --file=echo -- echo "hello world"`
1. `echo_hello_world`
1. `exit`

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
