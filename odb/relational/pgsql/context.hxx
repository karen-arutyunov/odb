// file      : odb/relational/pgsql/context.hxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_PGSQL_CONTEXT_HXX
#define ODB_RELATIONAL_PGSQL_CONTEXT_HXX

#include <odb/relational/context.hxx>

namespace relational
{
  namespace pgsql
  {
    struct sql_type
    {
      // Keep the order in each block of types.
      //
      enum core_type
      {
        // Integral types.
        //
        BOOLEAN,
        SMALLINT,
        INTEGER,
        BIGINT,

        // Float types.
        //
        REAL,
        DOUBLE,
        NUMERIC,

        // Data-time types.
        //
        DATE,
        TIME,
        TIMESTAMP,

        // String and binary types.
        //
        CHAR,
        VARCHAR,
        TEXT,
        BYTEA,
        BIT,
        VARBIT,

        // Other types.
        //
        UUID,

        // Invalid type.
        //
        invalid
      };

      sql_type () : type (invalid), range (false) {}

      core_type type;
      bool range;

      // VARBIT maximum length is 2^31 - 1 bit.
      // String types can hold a maximum of 1GB of data.
      //
      unsigned int range_value;
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
      database_type_impl (semantics::type& t, semantics::names* hint, bool);

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

#endif // ODB_RELATIONAL_PGSQL_CONTEXT_HXX
