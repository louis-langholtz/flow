# flow

A data-flow library and environment in C++.

## Status

[![linux](https://github.com/louis-langholtz/flow/actions/workflows/linux.yml/badge.svg)](https://github.com/louis-langholtz/flow/actions/workflows/linux.yml)

This is just a work in progress at the moment.

## What?

I'm using the phrase "data-flow" in terms of machines, programming, and systems.
In data-flow systems, instead of instructions or control moving through them
(like computers are typically thought of as doing),
the focus is on data moving through them.

See the references section below to find out more about data-flow in general.

At present, the project uses POSIX descriptors and signals to implement the flow
of data, and applications having well known descriptors and signals to implement
underlying leaf execution units. As abstractions, these conveniently fit the
data flow model and when instantiated execution is _asynchronous_.
See the [system.hpp](library/include/flow/system.hpp) header file for specifics
on how systems can be designed in C++.

At a conceptual design level, there are three types of things to think about:
ports, links, and nodes. Ports define data inputs or outputs.
Links define the binding together of output ports with input ports.
Nodes meanwhile encapsulate implementations taking the data from its input
ports, processing that data, and then outputting resulting data to its output
ports. These implementations can be executable programs, or systems that are
recursively definable containers of nodes connected via links to each other's
ports. When nodes are run, or _instantiated_, they're transformed into:
instances, and channels. Instances and channels exist until the instances exit.

## Why?

Data-flow programming and architecture has been an interest of mine for years.
Additionally, it has benefits that are more attractive than ever, like:
- Lending itself better to modern computing resources.
- Facilitating a more organic style of modeling natural processes.

## How?

Exactly how the project can be acquired, built, and run will depend on things
like having requirements and which components specifically you want to use.
Generally speaking, the following instructions hopefully suffice...

### Have Requirements

- POSIX-compliant OS (like Linux or macOS 10.5+).
- `git` command line tool.
- Compiler supporting the C++20 standard (or newer).
- CMake version 3.16.3 or newer command line tool.
- For the *shell* application, access to the _editline_
  "line editor and history library".
- For the *tests* application, access to https://github.com/google/googletest.

### Download Project Code

Assuming:
- You want to use the project's name of `flow` as the directory name under which
  to store the project.

From a terminal that's in the directory you want the `flow` sub-directory to
appear in, run the following:
1. `git clone https://github.com/louis-langholtz/flow.git`

### Build It

- To build all components into a separate sub-directory named `flow-build`
  (including compile commands for tools like `clang-tidy`), run:
  ```
  cmake -S flow -B flow-build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DFLOW_BUILD_SHELL=ON -DFLOW_BUILD_UNITTESTS=ON
  cmake --build flow-build
  ```
- Alternatively, or to find out more about each component (including some usage
  suggestions), see:
  - [The library component README.md](library/README.md).
  - [The shell application component README.md](shell/README.md).
  - [The tests application component README.md](tests/README.md).

## References

- Devopedia's [Dataflow Programming](https://devopedia.org/dataflow-programming) page.
- Wikipedia's [Dataflow programming](https://en.wikipedia.org/wiki/Dataflow_programming) page.
- Wikipedia's [POSIX](https://en.wikipedia.org/wiki/POSIX) page.
- Wikipedia's [Assembly theory](https://en.m.wikipedia.org/wiki/Assembly_theory).
