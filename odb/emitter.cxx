// file      : odb/emitter.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/emitter.hxx>

using namespace std;

void emitter::
pre ()
{
}

void emitter::
post ()
{
}

int emitter_ostream::streambuf::
sync ()
{
  string s (str ());

  // Get rid of the trailing newline if any.
  //
  if (string::size_type n = s.size ())
  {
    if (s[n - 1] == '\n')
      s.resize (n - 1);
  }

  e_.line (s);
  str (string ());
  return 0;
}
