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
