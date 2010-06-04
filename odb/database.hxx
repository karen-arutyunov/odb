// file      : odb/database.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_DATABASE_HXX
#define ODB_DATABASE_HXX

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

#endif // ODB_DATABASE_HXX
