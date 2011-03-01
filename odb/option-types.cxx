// file      : odb/option-types.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <string>
#include <istream>
#include <ostream>
#include <algorithm> // std::lower_bound

#include <odb/option-types.hxx>

using namespace std;

// database
//
static const char* database_[] =
{
  "mysql",
  "tracer"
};

const char* database::
string () const
{
  return database_[v_];
}

istream&
operator>> (istream& is, database& db)
{
  string s;
  is >> s;

  if (!is.fail ())
  {
    const char** e (database_ + sizeof (database_) / sizeof (char*));
    const char** i (lower_bound (database_, e, s));

    if (i != e && *i == s)
      db = database::value (i - database_);
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

// schema_format
//
static const char* schema_format_[] =
{
  "embedded",
  "sql"
};

const char* schema_format::
string () const
{
  return schema_format_[v_];
}

istream&
operator>> (istream& is, schema_format& sf)
{
  string s;
  is >> s;

  if (!is.fail ())
  {
    const char** e (schema_format_ + sizeof (schema_format_) / sizeof (char*));
    const char** i (lower_bound (schema_format_, e, s));

    if (i != e && *i == s)
      sf = schema_format::value (i - schema_format_);
    else
      is.setstate (istream::failbit);
  }

  return is;
}

ostream&
operator<< (ostream& os, schema_format sf)
{
  return os << sf.string ();
}
