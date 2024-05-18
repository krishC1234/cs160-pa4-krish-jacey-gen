# Assignment 4: Code Generation

__RELEASED:__ Tuesday, May 14
__DUE:__ Tuesday, May 28
__LATE DEADLINE 1:__ Tuesday, Jun 4 (-11%)
__LATE DEADLINE 2:__ Friday, Jun 14 (-31%)

You may work in groups of 1--3 people. If working in a group of 2--3, be sure to indicate that when submitting to Gradescope.

## Description

Implement a code generator for LIR targeting x86-64. The generator will take in a LIR program and output the corresponding x86-64 assembly instructions. The LIR inputs are guaranteed to be valid programs.

The input will be three files:

- The first input file is the LIR in JSON format. You can use the same C++ header for reading in JSON files as for assignment 3 (see the #assign-3 channel on Slack). Some notes on the LIR JSON format:

    - Every variable has a name, type, and optional scope. If the scope is present then it specifies the function that the variable belongs to. If the scope is not present then the variable is a global.

    - The Alloc instruction has an extra field called `id`; you can ignore this field, it isn't relevant for what we're doing.

- As a convenience, the second input file is the same program as a token stream, in the same format as used for the input of assign-2. If you are confident in your assign-2 and assign-3 solutions (the AST construction part of assign-2 and the lowering from assign-3), then you can reuse them to construct the LIR from the tokens instead of having to read in the LIR in JSON format.

- As a convenience, the third input file is the same program as an AST in JSON format, as used for assign-3. If you are confident in your assign-3 solution, then you can reuse it to construct the LIR from the AST instead of having to read in the LIR in JSON format.

All three files will be given as command-line arguments (e.g., `./codegen file_lir file_toks file_ast`); you only need to use one of them, they are all the same program just in different formats.

The output should be the ISA program resulting from codegen. Your ISA program must be the same as my solution (i.e., it is not sufficient to have a correct ISA program, it must be the same as mine in order to allow autograding). Your output must match the newlines in my solution (except ignoring completely blank lines), but otherwise whitespace will be ignored (i.e., the grader will be using `diff -wB` to compare your output to the solution).

- The `.out_of_bounds` and `.invalid_alloc_length` blocks described in lecture can be copied verbatim from the notes (or my solution's output), they are the same for all programs.

## Reference Solution

I have placed an executable of my own solution on CSIL in `~benh/160/codegen`. My solution only takes one file as an argument; that file must contain LIR in either JSON format _or_ the human-readable format used in lecture. For your convenient the `lower` executable on CSIL from assign-3 has been modified to take either the `-json` flag (output the LIR as JSON) or the `-hr` flag (output the LIR in human-readable format). You can use this reference solution to test your code before submitting. If you have any questions about the output format or the behavior of the code generator you can answer them using this reference solution as well; this is the solution that Gradescope will use to test your submissions.

## Submitting to Gradescope

The autograder is running Ubuntu 22.04 and has the latest `build-essential` package installed (including `g++` version 11.4.0 and the `make` utility). You may not use any other libraries or packages for your code. Your submission must meet the following requirements:

- There must be a `makefile` file s.t. invoking the `make` command builds your code, resulting in an executable called `codegen`. The `codegen` executable must take a single argument (the file to run the code generator on) and produce its output on standard out.

- If your solution contains sub-directories then the entire submission must be given as a zip file (this is required by Gradescope for submissions that contain a directory structure). The `makefile` should be in the root directory of the solution.

- The submitted files are not allowed to contain any binaries, and your solution is not allowed to use the network.

## Grading

The grading will consist of seven test suites:

- TS1: no structs, globals, externs, pointers, or any functions but `main` : 15 points

- TS2: as TS1 but add globals : 10 points

- TS3: as TS1 but add externs : 15 points

- TS4: as TS1 but add functions : 15 points

- TS5: as TS1 but add pointers : 15 points

- TS6: as TS5 but add structs : 15 points

- TS7: any valid program : 15 points