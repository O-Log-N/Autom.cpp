# Autom.cpp Style Guide

This is a style guide for Autom.cpp project. Many of the rules here are merely for consintency 
(a.k.a. "It just so happened that we want to do it this way"), so don't complain when something looks arbitrary -
it probably is.

## I.0 General

I.0.1 We're using very standard C++11.

I.0.2 **ALL** the files **MUST** start with the following notice (comment style MAY be changed for non-C++ files as necessary):

```
/*******************************************************************************
Copyright (C) 2016 <CONTRIBUTOR>
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/
```

**DON'T FORGET** to replace `<CONTRIBUTOR>` with **your name** (or name of your organization as applicable)

## I.1 Naming conventions

Naming conventions examples:

1. class TheVeryBestClass //also applies to structs, unions, and typedefs
2. void prettyMuchUnnecessaryFunction(); int myVar; //also apples to members
3. enum class BACKGROUND_COLOR { RED, GREEN, BLUE }; //see below on constants
4. #define MY_STRING "abracadabra" //see below on #defines
5. #ifndef FILE_NAME_INCLUDED //"include guard"
6. **MUST NOT** use identifiers starting with an underscore OR containing adjacent double underscore (technically reserved for C++)
7. Classes and functions which are used ONLY for debugging purposes (such as "dumpSomething()") SHOULD start with Dbg*/dbg*() respectively
8. file names: file_name.h

### I.2 On #includes

1. **MUST NOT** have any duplicate declarations for the same entity. In particular, function declarations in .cpp files are **PROHIBITED** for non-static functions
2. Each include **MUST** have "include guard" consisting of #ifndef FILE_NAME_INCLUDED - #define FILE_NAME_INCLUDED
    - #pragma once SHOULD NOT be used as non-standard
3. Rules for #includes are different for /include/ and /src/ folders:
  
  3.1 For /include/ folder:
    - **IMPORTANT**: We **SHOULD** minimize number of standard #includes within our /include/ folder
      - i.e. Each file in /include/ **SHOULD** use only the absolute minimum of #includes required for them to compile
    - ALL C++ standard includes **MUST** be included as `<include_file_name.h>`
        
  3.2 For /src/ folder, rules are different:
    - ALL C++ standard includes SHOULD be within /src/autom_cpp_include.h, and **MUST** be included as `<include_file_name.h>`
    - The very first #include file for ALL the files in /src/ SHOULD be `#include "../autom_cpp_include.h"` (with different number of '../' as necessary) 
4. *this space is intentionally left blank*
5. Our own includes **MUST** be included via "include_file_name.h" (with a relative path if necessary(!))

### I.3 On Constants and #defines

1. integer constants SHOULD be declared as C++11's enum class BACKGROUND_COLOR { RED, GREEN, BLUE } (and used as BACKGROUND_COLOR::RED etc.)
2. string constants MAY be declared via #define

### I.4 C++11 vs C++03 vs C

1. **MUST NOT** use C-style casts; use appropriate C++ *_cast<> instead
2. SHOULD use std containers where applicable
3. SHOULD NOT use pointers where possible, DO use references instead wherever applicable
4. SHOULD NOT use C-style typedef struct _X {} X; stuff. DO use C++-ush struct X {}; instead
5. SHOULD use C++11-style "range-based for loops" such as for(int i:v) {} where applicable
6. SHOULD use C++11 auto for iterators such as auto it = v.begin(); where applicable. This also SHOULD reduce the need for iterator typedefs
7. SHOULD NOT overuse C++11 auto for short-and-obvious types.
8. SHOULD use "f() = delete;" to prohibit calling functions (instead of C++03's declaring as private and not implementing)
9. namespace policy:
    - ALL Autom.cpp stuff **MUST** be within `autom` namespace
    - there MAY be nested namespaces (e.g. autom::fs)
10. **MUST** use `override` for all overridden functions
11. SHOULD use C++11 member initializers over constructors for 'member default' values 
12. noexcept use is DISCOURAGED except for moving constructors.

### I.5 Misc

1. iterations over the vector SHOULD use  size_t as index variable type: for(size_t i=0; i < v.size(); ++i)
2. prefixed increment SHOULD be used in standalone statements: for(auto it=v.begin();;++it) is preferred over for(auto it=v.begin();;it++)
3. *this space is intentionally left blank*
4. *this space is intentionally left blank*
5. non-constant global data **MUST NOT** be used (though in some cases, TLS MAY be needed)
   - if TLS is needed, C++11's thread_local modifier **MUST** be used
6. non-constant static data **MUST NOT** be used
7. const modifier SHOULD be used wherever applicable
    - const primitive types and const iterators SHOULD be passed by value, not by reference
8. **TODO: assert policy**
9. There **MUST NOT** be garbage in the program. All variables of primitive types **MUST** be initialized, including member variables. See I.4.11 about the proper way of initializing member variables.
10. *this space is intentionally left blank*
11. exceptions SHOULD be used over error codes. All exceptions **MUST** be derived from std::exception and **MUST** provide meaningful what() explanation (this explanation MAY be a constant string specific to this exception)

### I.6 astyle
1. All the C++ files SHOULD be run through astyle before committing to develop branch
    - it is normally NOT necessary for checkins within feature branches
2. Preferred astyle version: 2.05
3. astyle Options: none at the moment (everything goes by default)
