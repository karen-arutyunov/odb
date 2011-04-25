// file      : odb/error.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_ERROR_HXX
#define ODB_ERROR_HXX

#include <odb/gcc.hxx>

#include <iostream>

#include <cutl/fs/path.hxx>

using std::endl;

std::ostream&
error (cutl::fs::path const&, size_t line, size_t clmn);

std::ostream&
error (location_t);

inline std::ostream&
error ()
{
  return error (input_location);
}

#endif // ODB_ERROR_HXX
