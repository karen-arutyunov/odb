// file      : odb/diagnostics.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>
#include <odb/cxx-lexer.hxx>
#include <odb/diagnostics.hxx>

using namespace std;

std::ostream&
error (cutl::fs::path const& p, size_t line, size_t clmn)
{
  //@@ We only need to do this if we are still parsing (i.e.,
  //   pragma parsing). Is there a way to detect this?
  //
  errorcount++;

  cerr << p << ':' << line << ':' << clmn << ": error: ";
  return cerr;
}

std::ostream&
warn (cutl::fs::path const& p, size_t line, size_t clmn)
{
  warningcount++;

  cerr << p << ':' << line << ':' << clmn << ": warning: ";
  return cerr;
}

std::ostream&
info (cutl::fs::path const& p, size_t line, size_t clmn)
{
  cerr << p << ':' << line << ':' << clmn << ": info: ";
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

std::ostream&
warn (location_t loc)
{
  warningcount++;
  cerr << LOCATION_FILE (loc) << ':'
       << LOCATION_LINE (loc) << ':'
       << LOCATION_COLUMN (loc) << ':'
       << " warning: ";
  return cerr;
}

std::ostream&
info (location_t loc)
{
  cerr << LOCATION_FILE (loc) << ':'
       << LOCATION_LINE (loc) << ':'
       << LOCATION_COLUMN (loc) << ':'
       << " info: ";
  return cerr;
}

std::ostream&
error (cxx_lexer& l)
{
  return error (l.location ());
}

std::ostream&
warn (cxx_lexer& l)
{
  return warn (l.location ());
}

std::ostream&
info (cxx_lexer& l)
{
  return info (l.location ());
}

cutl::fs::path
location_file (location_t loc)
{
  return cutl::fs::path (LOCATION_FILE (loc));
}

size_t
location_line (location_t loc)
{
  return LOCATION_LINE (loc);
}

size_t
location_column (location_t loc)
{
  return LOCATION_COLUMN (loc);
}
