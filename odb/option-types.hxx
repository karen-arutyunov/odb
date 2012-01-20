// file      : odb/option-types.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_OPTION_TYPES_HXX
#define ODB_OPTION_TYPES_HXX

#include <iosfwd>

struct database
{
  enum value
  {
    // Keep in alphabetic order.
    //
    mssql,
    mysql,
    oracle,
    pgsql,
    sqlite
  };

  database (value v = value (0)) : v_ (v) {}
  operator value () const {return v_;}

  const char*
  string () const;

private:
  value v_;
};

std::istream&
operator>> (std::istream&, database&);

std::ostream&
operator<< (std::ostream&, database);

//
//
struct schema_format
{
  enum value
  {
    // Keep in alphabetic order.
    //
    embedded,
    sql
  };

  schema_format (value v = value (0)) : v_ (v) {}
  operator value () const {return v_;}

  const char*
  string () const;

private:
  value v_;
};

std::istream&
operator>> (std::istream&, schema_format&);

std::ostream&
operator<< (std::ostream&, schema_format);

//
//
struct oracle_version
{
  oracle_version (unsigned short major, unsigned short minor)
      : major_ (major), minor_ (minor)
  {
  }

  unsigned short
  ver_major () const
  {
    return major_;
  }

  unsigned short
  ver_minor () const
  {
    return minor_;
  }

private:
  unsigned short major_;
  unsigned short minor_;
};

inline bool
operator== (const oracle_version& x, const oracle_version& y)
{
  return x.ver_major () == y.ver_major ();
}

inline bool
operator!= (const oracle_version& x, const oracle_version& y)
{
  return !(x == y);
}

inline bool
operator< (const oracle_version& x, const oracle_version& y)
{
  return x.ver_major () < y.ver_major () ||
    (x.ver_major () == y.ver_major () &&
     x.ver_minor () <  y.ver_minor ());
}

inline bool
operator> (const oracle_version& x, const oracle_version& y)
{
  return x.ver_major () > y.ver_major () ||
    (x.ver_major () == y.ver_major () &&
     x.ver_minor () > y.ver_minor ());
}

inline bool
operator<= (const oracle_version& x, const oracle_version& y)
{
  return !(x > y);
}

inline bool
operator>= (const oracle_version& x, const oracle_version& y)
{
  return !(x < y);
}

std::istream&
operator>> (std::istream&, oracle_version&);

std::ostream&
operator<< (std::ostream&, oracle_version);

//
//
struct mssql_version
{
  mssql_version (unsigned short major, unsigned short minor)
      : major_ (major), minor_ (minor)
  {
  }

  unsigned short
  ver_major () const
  {
    return major_;
  }

  unsigned short
  ver_minor () const
  {
    return minor_;
  }

private:
  unsigned short major_;
  unsigned short minor_;
};

inline bool
operator== (const mssql_version& x, const mssql_version& y)
{
  return x.ver_major () == y.ver_major ();
}

inline bool
operator!= (const mssql_version& x, const mssql_version& y)
{
  return !(x == y);
}

inline bool
operator< (const mssql_version& x, const mssql_version& y)
{
  return x.ver_major () < y.ver_major () ||
    (x.ver_major () == y.ver_major () &&
     x.ver_minor () <  y.ver_minor ());
}

inline bool
operator> (const mssql_version& x, const mssql_version& y)
{
  return x.ver_major () > y.ver_major () ||
    (x.ver_major () == y.ver_major () &&
     x.ver_minor () > y.ver_minor ());
}

inline bool
operator<= (const mssql_version& x, const mssql_version& y)
{
  return !(x > y);
}

inline bool
operator>= (const mssql_version& x, const mssql_version& y)
{
  return !(x < y);
}

std::istream&
operator>> (std::istream&, mssql_version&);

std::ostream&
operator<< (std::ostream&, mssql_version);

#endif // ODB_OPTION_TYPES_HXX
