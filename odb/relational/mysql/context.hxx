// file      : odb/relational/mysql/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_MYSQL_CONTEXT_HXX
#define ODB_RELATIONAL_MYSQL_CONTEXT_HXX

#include <vector>

#include <odb/relational/context.hxx>

namespace relational
{
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
      std::vector<std::string> enumerators; // Enumerator strings for ENUM.
    };

    class context: public virtual relational::context
    {
    public:
      sql_type const&
      column_sql_type (semantics::data_member&,
                       string const& key_prefix = string ());

    protected:
      virtual bool
      grow_impl (semantics::class_&);

      virtual bool
      grow_impl (semantics::data_member&);

      virtual bool
      grow_impl (semantics::data_member&, semantics::type&, string const&);

    protected:
      virtual string
      quote_id_impl (string const&) const;

    protected:
      virtual string
      database_type_impl (semantics::type&, semantics::names*, bool);

    public:
      virtual
      ~context ();
      context ();
      context (std::ostream&, semantics::unit&, options_type const&);

      static context&
      current ()
      {
        return *current_;
      }

    private:
      static context* current_;

    private:
      struct data: base_context::data
      {
        data (std::ostream& os): base_context::data (os) {}
      };
      data* data_;
    };
  }
}

#endif // ODB_RELATIONAL_MYSQL_CONTEXT_HXX
