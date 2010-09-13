// file      : odb/database.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <string>
#include <istream>
#include <ostream>
#include <algorithm> // std::lower_bound

#include <odb/database.hxx>

using namespace std;

static const char* str[] =
{
  "mysql",
  "tracer"
};

const char* database::
string () const
{
  return str[v_];
}

istream&
operator>> (istream& is, database& db)
{
  string s;
  is >> s;

  if (!is.fail ())
  {
    const char** e (str + sizeof (str) / sizeof (char*));
    const char** i (lower_bound (str, e, s));

    if (i != e && *i == s)
      db = database::value (i - str);
    else
      is.setstate (istream::failbit);
  }

  return is;
}

ostream&
operator<< (ostream& os, database db)
{
  return os << db.string ();
}
