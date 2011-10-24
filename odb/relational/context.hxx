// file      : odb/relational/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_CONTEXT_HXX
#define ODB_RELATIONAL_CONTEXT_HXX

#include <odb/context.hxx>

#include <odb/semantics/relational.hxx>
#include <odb/traversal/relational.hxx>

namespace relational
{
  namespace sema_rel = semantics::relational;
  namespace trav_rel = traversal::relational;

  enum statement_kind
  {
    statement_select,
    statement_insert,
    statement_update,
    statement_where   // WHERE clause.
  };

  class context: public virtual ::context
  {
  public:
    // Return true if an object or value type has members for which
    // the image can grow.
    //
    bool
    grow (semantics::class_&);

    // The same for a member's value type.
    //
    bool
    grow (semantics::data_member&);

    bool
    grow (semantics::data_member&, semantics::type&, string const& key_prefix);

  public:
    // Quote SQL string.
    //
    string
    quote_string (string const&) const;

    // Quote SQL identifier.
    //
    string
    quote_id (string const&) const;

    // Quoted column and table names.
    //
    string
    column_qname (semantics::data_member& m) const
    {
      return quote_id (column_name (m));
    }

    string
    column_qname (data_member_path const& mp) const
    {
      return quote_id (column_name (mp));
    }

    string
    column_qname (semantics::data_member& m,
                  string const& key_prefix,
                  string const& default_name) const
    {
      return quote_id (column_name (m, key_prefix, default_name));
    }

    string
    table_qname (semantics::class_& c) const
    {
      return quote_id (table_name (c));
    }

    string
    table_qname (semantics::class_& obj, data_member_path const& mp) const
    {
      return quote_id (table_name (obj, mp));
    }

    string
    table_qname (semantics::data_member& m, table_prefix const& p) const
    {
      return quote_id (table_name (m, p));
    }

  protected:
    // The default implementation returns false.
    //
    virtual bool
    grow_impl (semantics::class_&);

    virtual bool
    grow_impl (semantics::data_member&);

    virtual bool
    grow_impl (semantics::data_member&,
               semantics::type&,
               string const&);

    // The default implementation uses the ISO quoting ('') and
    // escapes singe quotes inside the string by double-quoting
    // (' -> ''). Some (most?) database systems support escape
    // sequences. We may want to provide utilize that to support
    // things like \n, \t, etc.
    //
    virtual string
    quote_string_impl (string const&) const;

    // The default implementation uses the ISO quoting ("").
    //
    virtual string
    quote_id_impl (string const&) const;

  public:
    virtual
    ~context ();
    context ();

    static context&
    current ()
    {
      return *current_;
    }

  protected:
    struct data;
    typedef context base_context;

    context (data*, sema_rel::model*);

  private:
    static context* current_;

  protected:
    struct data: root_context::data
    {
      data (std::ostream& os): root_context::data (os) {}

      string bind_vector_;
      string truncated_vector_;
    };
    data* data_;

  public:
    sema_rel::model* model;

    bool generate_grow;
    bool need_alias_as;

    string const& bind_vector;
    string const& truncated_vector;
  };
}

#include <odb/relational/context.ixx>

#endif // ODB_RELATIONAL_CONTEXT_HXX
