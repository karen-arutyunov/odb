// file      : odb/relational/sqlite/context.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SQLITE_CONTEXT_HXX
#define ODB_RELATIONAL_SQLITE_CONTEXT_HXX

#include <map>

#include <odb/relational/context.hxx>

namespace relational
{
  namespace sqlite
  {
    struct sql_type
    {
      // Keep the order in each block of types.
      //
      enum core_type
      {
        INTEGER,
        REAL,
        TEXT,
        BLOB,
        invalid
      };

      sql_type (): type (invalid) {}

      core_type type;
    };

    class context: public virtual relational::context
    {
    public:
      sql_type const&
      parse_sql_type (string const&, semantics::data_member&);

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
      virtual bool
      grow_impl (semantics::class_&);

      virtual bool
      grow_impl (semantics::data_member&);

      virtual bool
      grow_impl (semantics::data_member&, semantics::type&, string const&);

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

        typedef std::map<string, sql_type> sql_type_cache;
        sql_type_cache sql_type_cache_;
      };

      data* data_;
    };
  }
}

#endif // ODB_RELATIONAL_SQLITE_CONTEXT_HXX
