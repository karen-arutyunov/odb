// file      : odb/relational/mssql/context.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_MSSQL_CONTEXT_HXX
#define ODB_RELATIONAL_MSSQL_CONTEXT_HXX

#include <odb/relational/context.hxx>

namespace relational
{
  namespace mssql
  {
    struct sql_type
    {
      // Keep the order in each block of types.
      //
      enum core_type
      {
        // Integral types.
        //
        BIT,
        TINYINT,
        SMALLINT,
        INT,
        BIGINT,

        // Fixed and floating point types.
        //
        DECIMAL,
        SMALLMONEY,
        MONEY,
        FLOAT,

        // String and binary types.
        //
        CHAR,
        VARCHAR,
        TEXT,

        NCHAR,
        NVARCHAR,
        NTEXT,

        BINARY,
        VARBINARY,
        IMAGE,

        // Date-time types.
        //
        DATE,
        TIME,
        DATETIME,
        DATETIME2,
        SMALLDATETIME,
        DATETIMEOFFSET,

        // Other types.
        //
        UNIQUEIDENTIFIER,
        ROWVERSION,

        // Invalid type.
        //
        invalid
      };

      sql_type () :
          type (invalid), has_prec (false), has_scale (false)
      {
      }

      core_type type;

      bool has_prec;
      unsigned short prec;  // Max numeric value is 8000. 0 indicates
                            // 'max' as in VARCHAR(max).
      bool has_scale;
      unsigned short scale; // Max value is 38.
    };

    class context: public virtual relational::context
    {
    public:
      sql_type const&
      column_sql_type (semantics::data_member&,
                       string const& key_prefix = string ());

    public:
      struct invalid_sql_type
      {
        invalid_sql_type (string const& message): message_ (message) {}

        string const&
        message () const {return message_;}

      private:
        string message_;
      };

      static sql_type
      parse_sql_type (string const&);

    protected:
      virtual string
      quote_id_impl (qname const&) const;

    protected:
      virtual string
      database_type_impl (semantics::type&, semantics::names*, bool);

    public:
      virtual
      ~context ();

      context ();
      context (std::ostream&,
               semantics::unit&,
               options_type const&,
               sema_rel::model*);

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

#endif // ODB_RELATIONAL_MSSQL_CONTEXT_HXX
