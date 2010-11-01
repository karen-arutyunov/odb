// file      : odb/type-processor.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_TYPE_PROCESSOR_HXX
#define ODB_TYPE_PROCESSOR_HXX

#include <odb/context.hxx>

// Issues diagnostics and throws generation_failed in case of an error.
//
void
process_types (context&);

#endif // ODB_TYPE_PROCESSOR_HXX
