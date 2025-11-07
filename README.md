[![StandWithUkraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://github.com/vshymanskyy/StandWithUkraine/blob/main/docs/README.md)

This fork exists to provide a secure installer and package manager experience.  
As a bonus feature, it adds regex support for `strcmp` and `repl` based on [Remimu](https://github.com/wareya/Remimu).  

The below reproduces part of the upstream project's README file.  
Unlike suggested therein, this fork does not aim at retaining pre-VC9 buildability.  

# Yori: CMD reimagined

## Intro

Yori is a CMD replacement shell that supports backquotes, job control, and improves tab completion, file matching, aliases, command history, and more.  It includes a handful of native Win32 tools that implement commonly needed tasks which can be used with any shell.

## Build

Compiling currently requires Visual C++, version 2 or newer.  To compile, run NMAKE.  Once compiled, YMAKE.EXE allows for more efficient subsequent compilation, using all cores in the machine.  For build options, run "NMAKE buildhelp".

## License

Yori is available under the MIT license.
