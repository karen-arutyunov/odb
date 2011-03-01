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

#endif // ODB_OPTION_TYPES_HXX
