# shell

A terminal-based shell application interface for this project.
This is just a work in progress at the moment.

## Goals

- [x] Having this application itself model an executable system from the outside, and model
      a custom system from the inside.
- [x] Providing a command line oriented language interfacing the underlying functionality.
- [ ] Having a well defined grammar.
- [ ] Having the grammar be semantically consistent. I.e. if there is a rule for adding
      something, there would be a rule for removing that something as well.
- [ ] Being approachable, as in being preferable and easier to use over other shells.
      And not just for recursive systems design, but even for every day use.
- [ ] Use conventional command line argument syntax. For example, see
      [POSIX Utility Conventions](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html)
      and
      [GNU Program Argument Syntax Conventions](https://www.gnu.org/software/libc/manual/html_node/Argument-Syntax.html).

## Requirements

- The flow library and headers.
- Compiler supporting C++20.
- POSIX-compliant operating system (like Linux or macOS 10.5+).
- Access to the _editline_ "line editor and history library".

