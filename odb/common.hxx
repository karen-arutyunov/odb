// file      : odb/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_COMMON_HXX
#define ODB_COMMON_HXX

#include <string>
#include <cstddef> // std::size_t

#include <odb/context.hxx>

// Traverse object members recursively by going into composite members.
//
struct object_members_base: traversal::class_, virtual context
{
  virtual void
  simple (semantics::data_member&);

  // If you override this function, call the base if you want the composite
  // to be recursively traversed. The second argument is the actual composite
  // type, which is not necessarily the same as m.type() in case of
  // traverse_composite().
  //
  virtual void
  composite (semantics::data_member&, semantics::class_&);

  virtual void
  container (semantics::data_member&);

public:
  object_members_base ()
      : member_ (*this)
  {
    init (false, false);
  }

  object_members_base (bool build_prefix, bool build_table_prefix)
      : member_ (*this)
  {
    init (build_prefix, build_table_prefix);
  }

  object_members_base (object_members_base const& x)
      : context (), //@@ -Wextra
        member_ (*this)
  {
    init (x.build_prefix_, x.build_table_prefix_);
  }

  virtual void
  traverse (semantics::class_&);

  virtual void
  traverse_composite (semantics::data_member&, semantics::class_&);

protected:
  std::string prefix_;
  context::table_prefix table_prefix_;

private:
  void
  init (bool build_prefix, bool build_table_prefix)
  {
    build_prefix_ = build_prefix;
    build_table_prefix_ = build_table_prefix;

    *this >> names_ >> member_;
    *this >> inherits_ >> *this;
  }

private:
  struct member: traversal::data_member
  {
    member (object_members_base& om)
        : om_ (om)
    {
    }

    virtual void
    traverse (semantics::data_member&);

  public:
    object_members_base& om_;
  };

  bool build_prefix_;
  bool build_table_prefix_;

  member member_;
  traversal::names names_;
  traversal::inherits inherits_;
};

// Traverse object columns recursively by going into composite members
// and bases.
//
struct object_columns_base: traversal::class_, virtual context
{
  // Returning false means that the column has been ignored and the
  // first flag should not be changed.
  //
  virtual bool
  column (semantics::data_member&, std::string const& name, bool first) = 0;

  // If you override this function, you can call the base to traverse
  // bases and members. The first argument is the data member and can
  // be NULL if we are traversing the root type or a base. The second
  // argument is the actual composite type, which is not necessarily
  // the same as m.type ().
  //
  virtual void
  composite (semantics::data_member*, semantics::class_&);

  // If you override this function, you can call the base to traverse
  // bases and members.
  //
  virtual void
  object (semantics::class_&);

  // Called after the last column, provided at least one column hasn't
  // been ignored.
  //
  virtual void
  flush ();

public:
  object_columns_base ()
      : top_level_ (true), member_ (*this)
  {
    init ();
  }

  object_columns_base (object_columns_base const&)
      : context (), //@@ -Wextra
        top_level_ (true),
        member_ (*this)
  {
    init ();
  }

  virtual void
  traverse (semantics::class_&);

  virtual void
  traverse_composite (semantics::data_member&,
                      semantics::class_&,
                      std::string const& key_prefix,
                      std::string const& default_name);
private:
  void
  init ()
  {
    *this >> names_ >> member_;
    *this >> inherits_ >> *this;
  }

private:
  struct member: traversal::data_member, context
  {
    member (object_columns_base& oc)
        : oc_ (oc), first_ (true)
    {
    }

    virtual void
    traverse (semantics::data_member&);

  public:
    object_columns_base& oc_;

    string prefix_;
    bool first_;
  };

  bool top_level_;

  member member_;
  traversal::names names_;
  traversal::inherits inherits_;
};

#endif // ODB_COMMON_HXX
