# tests

A google-test based unit testing application for this project.

## Requirements

- The flow library and headers.
- Compiler supporting C++20.
- POSIX-compliant operating system (like Linux or macOS 10.5+).
- Access to https://github.com/google/googletest.

## Build It

Assuming:
- You have downloaded the project code and it's in the directory named `flow`.
  If not, follow the Download Project Code instructions in the top-level
  [README.md](../README.md) file, then return here.
- You're in the directory containing the `flow` directory.
- You want to build this component in a separate directory named `flow-build`.

From a terminal that's in the directory you want `flow-build` to appear in, run the following:
1. `cmake -S flow -B flow-build -DFLOW_BUILD_UNITTESTS=ON`
1. `cmake --build flow-build`

## Run It

From a terminal that's in the directory containing `flow-build`, run:
1. `./flow-build/bin/tests`
1. See the output.

