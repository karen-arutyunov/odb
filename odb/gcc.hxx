// file      : odb/gcc.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_GCC_TREE_HXX
#define ODB_GCC_TREE_HXX

// GCC header includes to get the parse tree declarations. The order
// is important and doesn't follow any kind of logic.
//

#include <stdlib.h>
#include <gmp.h>

#include <cstdlib> // Include before GCC poisons some declarations.

extern "C"
{
#include "gcc-plugin.h"

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "intl.h"

#include "tm.h"

#include "diagnostic.h"
#include "c-common.h"
#include "cp/cp-tree.h"
}

#endif // ODB_GCC_TREE_HXX
