# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /home/grigory/Programs/CLion/clion-2019.2.4/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /home/grigory/Programs/CLion/clion-2019.2.4/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/grigory/Abschlussprojekt/open62541-fork

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/grigory/Abschlussprojekt/open62541-fork

# Utility rule file for open62541-generator-types-di.

# Include the progress variables for this target.
include examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/progress.make

examples/nodeset/CMakeFiles/open62541-generator-types-di: src_generated/open62541/types_di_generated.c
examples/nodeset/CMakeFiles/open62541-generator-types-di: src_generated/open62541/types_di_generated.h
examples/nodeset/CMakeFiles/open62541-generator-types-di: src_generated/open62541/types_di_generated_handling.h
examples/nodeset/CMakeFiles/open62541-generator-types-di: src_generated/open62541/types_di_generated_encoding_binary.h


src_generated/open62541/types_di_generated.c: tools/generate_datatypes.py
src_generated/open62541/types_di_generated.c: deps/ua-nodeset/DI/Opc.Ua.Di.Types.bsd
src_generated/open62541/types_di_generated.c: deps/ua-nodeset/DI/OpcUaDiModel.csv
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/grigory/Abschlussprojekt/open62541-fork/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating ../../src_generated/open62541/types_di_generated.c, ../../src_generated/open62541/types_di_generated.h, ../../src_generated/open62541/types_di_generated_handling.h, ../../src_generated/open62541/types_di_generated_encoding_binary.h"
	cd /home/grigory/Abschlussprojekt/open62541-fork/examples/nodeset && /usr/bin/python /home/grigory/Abschlussprojekt/open62541-fork/tools/generate_datatypes.py --namespace=2 --type-bsd=/home/grigory/Abschlussprojekt/open62541-fork/deps/ua-nodeset/DI/Opc.Ua.Di.Types.bsd --type-csv=/home/grigory/Abschlussprojekt/open62541-fork/deps/ua-nodeset/DI/OpcUaDiModel.csv --no-builtin /home/grigory/Abschlussprojekt/open62541-fork/src_generated/open62541/types_di

src_generated/open62541/types_di_generated.h: src_generated/open62541/types_di_generated.c
	@$(CMAKE_COMMAND) -E touch_nocreate src_generated/open62541/types_di_generated.h

src_generated/open62541/types_di_generated_handling.h: src_generated/open62541/types_di_generated.c
	@$(CMAKE_COMMAND) -E touch_nocreate src_generated/open62541/types_di_generated_handling.h

src_generated/open62541/types_di_generated_encoding_binary.h: src_generated/open62541/types_di_generated.c
	@$(CMAKE_COMMAND) -E touch_nocreate src_generated/open62541/types_di_generated_encoding_binary.h

open62541-generator-types-di: examples/nodeset/CMakeFiles/open62541-generator-types-di
open62541-generator-types-di: src_generated/open62541/types_di_generated.c
open62541-generator-types-di: src_generated/open62541/types_di_generated.h
open62541-generator-types-di: src_generated/open62541/types_di_generated_handling.h
open62541-generator-types-di: src_generated/open62541/types_di_generated_encoding_binary.h
open62541-generator-types-di: examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/build.make

.PHONY : open62541-generator-types-di

# Rule to build all files generated by this target.
examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/build: open62541-generator-types-di

.PHONY : examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/build

examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/clean:
	cd /home/grigory/Abschlussprojekt/open62541-fork/examples/nodeset && $(CMAKE_COMMAND) -P CMakeFiles/open62541-generator-types-di.dir/cmake_clean.cmake
.PHONY : examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/clean

examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/depend:
	cd /home/grigory/Abschlussprojekt/open62541-fork && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/grigory/Abschlussprojekt/open62541-fork /home/grigory/Abschlussprojekt/open62541-fork/examples/nodeset /home/grigory/Abschlussprojekt/open62541-fork /home/grigory/Abschlussprojekt/open62541-fork/examples/nodeset /home/grigory/Abschlussprojekt/open62541-fork/examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/nodeset/CMakeFiles/open62541-generator-types-di.dir/depend

