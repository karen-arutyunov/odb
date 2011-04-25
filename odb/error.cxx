// file      : odb/error.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <iostream>

#include <odb/error.hxx>

using namespace std;

std::ostream&
error (cutl::fs::path const& p, size_t line, size_t clmn)
{
  errorcount++;
  cerr << p << ':' << line << ':' << clmn << ": error: ";
  return cerr;
}

std::ostream&
error (location_t loc)
{
  errorcount++;
  cerr << LOCATION_FILE (loc) << ':'
       << LOCATION_LINE (loc) << ':'
       << LOCATION_COLUMN (loc) << ':'
       << " error: ";
  return cerr;
}
