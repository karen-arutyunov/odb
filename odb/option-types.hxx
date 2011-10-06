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
    mysql,
    oracle,
    pgsql,
    sqlite,
    tracer
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
  oracle_version () {}
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

  bool
  equal (const oracle_version& x) const
  {
    return major_ == x.major_ && minor_ == x.minor_;
  }

  bool
  less (const oracle_version& x) const
  {
    return major_ < x.major_ || (major_ == x.major_ && minor_ <  x.minor_);
  }

  bool
  greater (const oracle_version& x) const
  {
    return major_ > x.major_ || (major_ == x.major_ && minor_ > x.minor_);
  }

private:
  unsigned short major_;
  unsigned short minor_;
};

bool
inline operator< (const oracle_version& x, const oracle_version& y)
{
  return x.less (y);
}

bool
inline operator> (const oracle_version& x, const oracle_version& y)
{
  return x.greater (y);
}

bool
inline operator<= (const oracle_version& x, const oracle_version& y)
{
  return !x.greater (y);
}

bool
inline operator>= (const oracle_version& x, const oracle_version& y)
{
  return !x.less (y);
}

std::istream&
operator>> (std::istream&, oracle_version&);

std::ostream&
operator<< (std::ostream&, oracle_version);

#endif // ODB_OPTION_TYPES_HXX
