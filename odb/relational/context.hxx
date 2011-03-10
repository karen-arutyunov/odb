// file      : odb/relational/context.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_CONTEXT_HXX
#define ODB_RELATIONAL_CONTEXT_HXX

#include <odb/context.hxx>

namespace relational
{
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
    // Quote SQL identifier.
    //
    string
    quote_id (string const&) const;

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

    context (data*);

  private:
    static context* current_;

  protected:
    struct data: root_context::data
    {
      data (std::ostream& os): root_context::data (os) {}
    };
    data* data_;
  };
}

#include <odb/relational/context.ixx>

#endif // ODB_RELATIONAL_CONTEXT_HXX
