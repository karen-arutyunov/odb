// file      : odb/relational/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_COMMON_HXX
#define ODB_RELATIONAL_COMMON_HXX

#include <map>
#include <cstddef> // std::size_t
#include <cassert>
#include <typeinfo>

#include <odb/common.hxx>
#include <odb/relational/context.hxx>

namespace relational
{
  struct member_base: traversal::data_member, virtual context
  {
    typedef member_base base;

    member_base (semantics::type* type,
                 string const& fq_type,
                 string const& key_prefix)
        : type_override_ (type),
          fq_type_override_ (fq_type),
          key_prefix_ (key_prefix)
    {
    }

    member_base (string const& var,
                 semantics::type* type,
                 string const& fq_type,
                 string const& key_prefix)
        : var_override_ (var),
          type_override_ (type),
          fq_type_override_ (fq_type),
          key_prefix_ (key_prefix)
    {
    }

  protected:
    // For virtual inheritance only. Should not be actually called.
    //
    member_base (); // {assert (false);}

  protected:
    string var_override_;
    semantics::type* type_override_;
    string fq_type_override_;
    string key_prefix_;
  };

  //
  //
  struct query_columns_base: object_columns_base, virtual context
  {
    typedef query_columns_base base;

    query_columns_base ();
    query_columns_base (semantics::class_&);

    virtual void
    traverse_object (semantics::class_&);

    virtual void
    traverse_composite (semantics::data_member*, semantics::class_&);

    virtual bool
    traverse_column (semantics::data_member&, string const&, bool);

  protected:
    bool decl_;
    string scope_;
  };

  //
  //
  struct query_columns: object_columns_base, virtual context
  {
    typedef query_columns base;

    query_columns (bool ptr);
    query_columns (bool ptr, semantics::class_&);

    virtual string
    database_type_id (semantics::data_member&)
    {
      assert (false);
    }

    virtual void
    traverse_object (semantics::class_&);

    virtual void
    traverse_composite (semantics::data_member*, semantics::class_&);

    virtual bool
    traverse_column (semantics::data_member&, string const&, bool);

  protected:
    bool ptr_;
    bool decl_;

    string scope_;
    string table_;
    string default_table_;
  };

  //
  // Dynamic traversal support.
  //

  template <typename B>
  struct factory
  {
    static B*
    create (B const& prototype)
    {
      database db (context::current ().options.database ());

      if (map_ != 0)
      {
        typename map::const_iterator i (map_->find (db));

        if (i != map_->end ())
          return i->second (prototype);
      }

      return new B (prototype);
    }

  private:
    template <typename>
    friend struct entry;

    static void
    init ()
    {
      if (factory<B>::count_++ == 0)
        factory<B>::map_ = new typename factory<B>::map;
    }

    static void
    term ()
    {
      if (--factory<B>::count_ == 0)
        delete factory<B>::map_;
    }

    typedef B* (*create_func) (B const&);
    typedef std::map<database, create_func> map;
    static map* map_;
    static std::size_t count_;
  };

  template <typename B>
  typename factory<B>::map* factory<B>::map_;

  template <typename B>
  std::size_t factory<B>::count_;

  struct entry_base
  {
    static database
    db (std::type_info const&);
  };

  template <typename D>
  struct entry: entry_base
  {
    typedef typename D::base base;

    entry ()
    {
      factory<base>::init ();
      (*factory<base>::map_)[db (typeid (D))] = &create;
    }

    ~entry ()
    {
      factory<base>::term ();
    }

    static base*
    create (base const& prototype)
    {
      return new D (prototype);
    }
  };

  template <typename B>
  struct instance
  {
    typedef relational::factory<B> factory;

    ~instance ()
    {
      delete x_;
    }

    instance ()
    {
      B prototype;
      x_ = factory::create (prototype);
    }

    template <typename A1>
    instance (A1& a1)
    {
      B prototype (a1);
      x_ = factory::create (prototype);
    }

    template <typename A1>
    instance (A1 const& a1)
    {
      B prototype (a1);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2>
    instance (A1& a1, A2& a2)
    {
      B prototype (a1, a2);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2>
    instance (A1 const& a1, A2 const& a2)
    {
      B prototype (a1, a2);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2, typename A3>
    instance (A1& a1, A2& a2, A3& a3)
    {
      B prototype (a1, a2, a3);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2, typename A3>
    instance (A1 const& a1, A2 const& a2, A3 const& a3)
    {
      B prototype (a1, a2, a3);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2, typename A3, typename A4>
    instance (A1& a1, A2& a2, A3& a3, A4& a4)
    {
      B prototype (a1, a2, a3, a4);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2, typename A3, typename A4>
    instance (A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4)
    {
      B prototype (a1, a2, a3, a4);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2, typename A3, typename A4, typename A5>
    instance (A1& a1, A2& a2, A3& a3, A4& a4, A5& a5)
    {
      B prototype (a1, a2, a3, a4, a5);
      x_ = factory::create (prototype);
    }

    template <typename A1, typename A2, typename A3, typename A4, typename A5>
    instance (A1 const& a1, A2 const& a2, A3 const& a3, A4 const& a4,
              A5 const& a5)
    {
      B prototype (a1, a2, a3, a4, a5);
      x_ = factory::create (prototype);
    }

    instance (instance const& i)
    {
      // This is tricky: use the other instance as a prototype.
      //
      x_ = factory::create (*i.x_);
    }

    B*
    operator-> () const
    {
      return x_;
    }

    B&
    operator* () const
    {
      return *x_;
    }

    B*
    get () const
    {
      return x_;
    }

  private:
    instance& operator= (instance const&);

  private:
    B* x_;
  };

  template <typename T>
  inline traversal::edge_base&
  operator>> (instance<T>& n, traversal::edge_base& e)
  {
    n->edge_traverser (e);
    return e;
  }

  template <typename T>
  inline traversal::relational::edge_base&
  operator>> (instance<T>& n, traversal::relational::edge_base& e)
  {
    n->edge_traverser (e);
    return e;
  }

  template <typename T>
  inline traversal::node_base&
  operator>> (traversal::edge_base& e, instance<T>& n)
  {
    e.node_traverser (*n);
    return *n;
  }

  template <typename T>
  inline traversal::relational::node_base&
  operator>> (traversal::relational::edge_base& e, instance<T>& n)
  {
    e.node_traverser (*n);
    return *n;
  }
}

#endif // ODB_RELATIONAL_COMMON_HXX
