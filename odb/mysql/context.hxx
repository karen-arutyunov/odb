// file      : odb/mysql/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_MYSQL_CONTEXT_HXX
#define ODB_MYSQL_CONTEXT_HXX

#include <string>

#include <odb/context.hxx>

namespace mysql
{
  struct sql_type
  {
    sql_type () : unsign (false), bounds (false) {}

    std::string type;
    bool unsign;
    bool bounds;
    unsigned int bounds_value; // MySQL max value is 2^32 - 1 (LONGBLOG/TEXT).
  };

  class context: public ::context
  {
  private:
    typedef ::context base_context;

    struct data: base_context::data
    {
    };

  private:
    data* data_;

  public:
    sql_type const&
    db_type (semantics::data_member&);

  public:
    context (std::ostream&, semantics::unit&, options_type const&);
    context (context&);
  };
}

#endif // ODB_MYSQL_CONTEXT_HXX
