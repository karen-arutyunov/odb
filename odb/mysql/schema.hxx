// file      : odb/mysql/schema.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_MYSQL_SCHEMA_HXX
#define ODB_MYSQL_SCHEMA_HXX

#include <odb/mysql/context.hxx>

namespace mysql
{
  void
  generate_schema (context&);
}

#endif // ODB_MYSQL_SCHEMA_HXX
