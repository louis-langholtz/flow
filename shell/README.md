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

## Build It

Assuming:
- You have downloaded the project code and it's in the directory named `flow`.
  If not, follow the Download Project Code instructions in the top-level
  [README.md](../README.md) file, then return here.
- You're in the directory containing the `flow` directory.
- You want to build this component in a separate directory named `flow-build`.

From a terminal that's in the directory you want `flow-build` to appear in, run the following:
1. `cmake -S flow -B flow-build -DFLOW_BUILD_SHELL=ON`
1. `cmake --build flow-build`

## Startup Command Line Shell And Get Help

From a terminal that's in the directory containing `flow-build`, run:
1. `./flow-build/bin/shell`
1. `help`

This shows the various builtin flow commands that the shell knows about and that you can run.
It also leaves you in the shell and ready to run other commands like the following.
If you don't want to run other commands, just enter `exit`.

## Run Hello World And Exit

From within the command line shell,
define a system for echoing "hello world",
instantiate it in the foreground,
and then exit:
1. `systems set echo_hello_world --file=echo -- echo "hello world"`
1. `echo_hello_world`
1. `exit`

## Bigger Example

```
systems set cat_system {}
systems set xargs_system {}
connections add :1@cat_system-:0@xargs_system
connections add :2@cat_system-%/dev/null
connections add :2@xargs_system-%/dev/null
connections add ^stdin-:0@cat_system
connections add :1@xargs_system-^stdout
pop --rebase=sys
sys &
```
