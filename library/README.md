# library

This is the project's programming library and main component.
All other components depend on this one.

As is conventional:
- The [include](include/) directory is the directory where user
  accessible header files of the library reside.
- The [source](source/) directory is the directory where non-user
  accessible files of the library - like source code files - reside.

## Goals

- [x] Providing mechanisms for defining and using systems based on
      a subset of POSIX standard facilities providing convenient
      abstractions for dataflow systems.
- [ ] Extending these mechanisms to the most efficient and beneficial
      facilities available.
- [ ] Defining a widely supportable machine model that all
      components of this project are based upon.

## Build It

Assuming:
- You have downloaded the project code and it's in the directory named `flow`.
  If not, follow the Download Project Code instructions in the top-level
  [README.md](../README.md) file, then return here.
- You're in the directory containing the `flow` directory.
- You want to build this component in a separate directory named `flow-build`.

From a terminal that's in the directory you want `flow-build` to appear in, run the following:
1. `cmake -S flow -B flow-build`
1. `cmake --build flow-build`
