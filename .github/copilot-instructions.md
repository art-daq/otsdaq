# Project Overview

_otsdaq_ is a Ready-to-Use data-acquisition (DAQ) solution aimed at scaling down to test-beam, detector development, and other rapid-deployment scenarios; and scaling up through the development cycle to fullscale production and operation.   _otsdaq_ uses the _artdaq_ DAQ framework under-the-hood, providing flexibility and scalability to meet evolving DAQ needs.

## Libraries and Frameworks

otsdaq is built on the art, artdaq, and XDAQ frameworks, which provide a modular and flexible architecture for data processing. It also utilizes various C++ libraries for tasks such as serialization, logging, and configuration management.

otsdaq is built using CMake and the Spack package manager. A build environment is provided in the `ghcr.io/art-daq/otsdaq-spack` Docker image, which includes all necessary dependencies and tools for building and running otsdaq. The build environment is located at /opt/otsdaq within the container.

## Coding Standards

All C++ source files must be formatted with `clang-format`. C/C++ header files should be formatted with `clang-format` using the `.clang-format-hpp` instruction file if it exists, `.clang-format` otherwise. All python files must be formatted with `black`. The project uses the Google C++ Style Guide as a reference for coding standards, with some modifications to fit the specific needs of the otsdaq framework. Developers are encouraged to follow these guidelines to maintain consistency and readability across the codebase.
