// file      : odb/mysql/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_MYSQL_CONTEXT_HXX
#define ODB_MYSQL_CONTEXT_HXX

#include <string>

#include <odb/context.hxx>

namespace mysql
{
  struct sql_type
  {
    // Keep the order in each block of types.
    //
    enum core_type
    {
      // Integral types.
      //
      TINYINT,
      SMALLINT,
      MEDIUMINT,
      INT,
      BIGINT,

      // Float types.
      //
      FLOAT,
      DOUBLE,
      DECIMAL,

      // Data-time types.
      //
      DATE,
      TIME,
      DATETIME,
      TIMESTAMP,
      YEAR,

      // String and binary types.
      //
      CHAR,
      BINARY,
      VARCHAR,
      VARBINARY,
      TINYTEXT,
      TINYBLOB,
      TEXT,
      BLOB,
      MEDIUMTEXT,
      MEDIUMBLOB,
      LONGTEXT,
      LONGBLOB,

      // Other types.
      //
      BIT,
      ENUM,
      SET,

      // Invalid type.
      //
      invalid
    };

    sql_type () : type (invalid), unsign (false), range (false) {}

    core_type type;
    bool unsign;
    bool range;
    unsigned int range_value; // MySQL max value is 2^32 - 1 (LONGBLOG/TEXT).
  };

  class context: public ::context
  {
    // Predicates.
    //
  public:

    // Return true if an object or value type has members for which
    // the image can grow.
    //
    bool
    grow (semantics::class_&);

    //
    //
  public:
    sql_type const&
    db_type (semantics::data_member&);

  private:
    typedef ::context base_context;

    struct data: base_context::data
    {
    };

  private:
    data* data_;

  public:
    virtual string
    column_type_impl (semantics::data_member&) const;

  public:
    context (std::ostream&, semantics::unit&, options_type const&);
    context (context&);
  };
}

#endif // ODB_MYSQL_CONTEXT_HXX
