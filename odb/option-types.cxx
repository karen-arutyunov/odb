// file      : odb/option-types.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <string>
#include <istream>
#include <ostream>
#include <algorithm> // std::lower_bound

#include <odb/option-types.hxx>

using namespace std;

//
// cxx_version
//

static const char* cxx_version_[] =
{
  "c++98",
  "c++11"
};

const char* cxx_version::
string () const
{
  return cxx_version_[v_];
}

istream&
operator>> (istream& is, cxx_version& v)
{
  string s;
  is >> s;

  if (!is.fail ())
  {
    if (s == "c++98")
      v = cxx_version::cxx98;
    else if (s == "c++11")
      v = cxx_version::cxx11;
    else
      is.setstate (istream::failbit);
  }

  return is;
}

//
// database
//

static const char* database_[] =
{
  "mssql",
  "mysql",
  "oracle",
  "pgsql",
  "sqlite"
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

//
// schema_format
//

static const char* schema_format_[] =
{
  "embedded",
  "separate",
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

//
// oracle_version
//

istream&
operator>> (istream& is, oracle_version& v)
{
  unsigned short major, minor;

  // Extract the major version.
  //
  is >> major;

  if (!is.fail ())
  {
    // Extract the decimal point.
    //
    char p;
    is >> p;

    if (!is.fail () && p == '.')
    {
      // Extract the minor version.
      //
      is >> minor;

      if (!is.fail ())
        v = oracle_version (major, minor);
    }
    else
      is.setstate (istream::failbit);
  }

  return is;
}

ostream&
operator<< (ostream& os, oracle_version v)
{
  return os << v.ver_major () << '.' << v.ver_minor ();
}

//
// mssql_version
//

istream&
operator>> (istream& is, mssql_version& v)
{
  unsigned short major, minor;

  // Extract the major version.
  //
  is >> major;

  if (!is.fail ())
  {
    // Extract the decimal point.
    //
    char p;
    is >> p;

    if (!is.fail () && p == '.')
    {
      // Extract the minor version.
      //
      is >> minor;

      if (!is.fail ())
        v = mssql_version (major, minor);
    }
    else
      is.setstate (istream::failbit);
  }

  return is;
}

ostream&
operator<< (ostream& os, mssql_version v)
{
  return os << v.ver_major () << '.' << v.ver_minor ();
}
