// file      : odb/gcc.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_GCC_HXX
#define ODB_GCC_HXX

#include <odb/gcc-fwd.hxx>

// GCC header includes to get the plugin and parse tree declarations.
// The order is important and doesn't follow any kind of logic.
//

#include <stdlib.h>
#include <gmp.h>

#include <cstdlib> // Include before GCC poisons some declarations.

#include <bversion.h>

#if BUILDING_GCC_MAJOR == 4 && BUILDING_GCC_MINOR <= 6
extern "C"
{
#endif

#include <gcc-plugin.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tree.h>
#include <intl.h>
#include <real.h>

#include <tm.h>

#include <cpplib.h>
#include <cp/cp-tree.h>

#if BUILDING_GCC_MAJOR > 4 || BUILDING_GCC_MAJOR == 4 && BUILDING_GCC_MINOR > 5
#  include <c-family/c-common.h>
#  include <c-family/c-pragma.h>
#else
#  include <c-common.h>
#  include <c-pragma.h>
#endif

#include <diagnostic.h>
#include <output.h>

#if BUILDING_GCC_MAJOR == 4 && BUILDING_GCC_MINOR <= 6
} // extern "C"
#endif

#ifndef LOCATION_COLUMN
#define LOCATION_COLUMN(LOC) (expand_location (LOC).column)
#endif

#ifndef DECL_SOURCE_COLUMN
#define DECL_SOURCE_COLUMN(NODE) LOCATION_COLUMN (DECL_SOURCE_LOCATION (NODE))
#endif

#endif // ODB_GCC_HXX
