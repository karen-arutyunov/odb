// file      : odb/relational/common.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_COMMON_HXX
#define ODB_RELATIONAL_COMMON_HXX

#include <map>
#include <set>
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
    member_base ();

  protected:
    string var_override_;
    semantics::type* type_override_;
    string fq_type_override_;
    string key_prefix_;
  };

  // Template argument is the database SQL type (sql_type).
  //
  template <typename T>
  struct member_base_impl: virtual member_base
  {
    typedef member_base_impl base_impl;

    member_base_impl (base const& x): base (x) {}

  protected:
    member_base_impl () {}

  public:
    virtual T const&
    member_sql_type (semantics::data_member&) = 0;

    virtual void
    traverse (semantics::data_member&);

    struct member_info
    {
      semantics::data_member& m; // Member.
      semantics::type& t;        // Cvr-unqualified member C++ type, note
                                 // that m.type () may not be the same as t.
      semantics::class_* ptr;    // Pointed-to object if m is an object
                                 // pointer. In this case t is the id type
                                 // while fq_type_ is the pointer fq-type.
      semantics::type* wrapper;  // Wrapper type if member is a composite or
                                 // container wrapper, also cvr-unqualified.
                                 // In this case t is the wrapped type.
      bool cq;                   // True if the original (wrapper) type
                                 // is const-qualified.
      T const* st;               // Member SQL type (only simple values).
      string& var;               // Member variable name with trailing '_'.

      // C++ type fq-name.
      //
      string
      fq_type (bool unwrap = true) const
      {
        semantics::names* hint;

        if (wrapper != 0 && unwrap)
        {
          // Use the hint from the wrapper unless the wrapped type
          // is qualified.
          //
          hint = wrapper->get<semantics::names*> ("wrapper-hint");
          utype (*context::wrapper (*wrapper), hint);
          return t.fq_name (hint);
        }

        // Use the original type from 'm' instead of 't' since the hint may
        // be invalid for a different type. Plus, if a type is overriden,
        // then the fq_type must be as well.
        //
        if (ptr != 0)
        {
          semantics::type& t (utype (*id_member (*ptr), hint));
          return t.fq_name (hint);
        }
        else if (fq_type_.empty ())
        {
          semantics::type& t (utype (m, hint));
          return t.fq_name (hint);
        }
        else
          return fq_type_;
      }

      string
      ptr_fq_type () const
      {
        assert (ptr != 0);

        if (fq_type_.empty ())
        {
          // If type is overridden so should fq_type so it is safe to
          // get the type from the member.
          //
          semantics::names* hint;
          semantics::type& t (utype (m, hint));
          return t.fq_name (hint);
        }
        else
          return fq_type_;
      }

      string const& fq_type_;

      member_info (semantics::data_member& m_,
                   semantics::type& t_,
                   semantics::type* wrapper_,
                   bool cq_,
                   string& var_,
                   string const& fq_type)
          : m (m_),
            t (t_),
            ptr (0),
            wrapper (wrapper_),
            cq (cq_),
            st (0),
            var (var_),
            fq_type_ (fq_type)
      {
      }
    };

    bool
    container (member_info& mi)
    {
      // This cannot be a container if we have a type override.
      //
      return type_override_ == 0 && context::container (mi.m);
    }

    // The false return value indicates that no further callbacks
    // should be called for this member.
    //
    virtual bool
    pre (member_info&)
    {
      return true;
    }

    virtual void
    post (member_info&)
    {
    }

    virtual void
    traverse_composite (member_info&)
    {
    }

    virtual void
    traverse_container (member_info&)
    {
    }

    // Note that by default traverse_object_pointer() will traverse the
    // pointed-to object id type.
    //
    virtual void
    traverse_object_pointer (member_info&);

    virtual void
    traverse_simple (member_info&) = 0;
  };

  //
  //
  struct member_database_type_id: virtual member_base
  {
    typedef member_database_type_id base;

    member_database_type_id (semantics::type* type = 0,
                             string const& fq_type = string (),
                             string const& key_prefix = string ())
        : member_base (type, fq_type, key_prefix)
    {
    }

    virtual string
    database_type_id (semantics::data_member&)
    {
      assert (false);
    }
  };

  // Generate alias_traits specializations for pointers in this objects.
  //
  struct query_alias_traits: object_columns_base, virtual context
  {
    typedef query_alias_traits base;

    query_alias_traits (semantics::class_&, bool decl);

    virtual void
    traverse_object (semantics::class_&);

    virtual void
    traverse_composite (semantics::data_member*, semantics::class_&);

    virtual void
    traverse_pointer (semantics::data_member&, semantics::class_&);

    virtual void
    generate_decl (string const& tag, semantics::class_&);

    virtual void
    generate_def (string const& tag, semantics::class_&, string const& alias);

  protected:
    bool decl_;
    string scope_;
  };

  //
  //
  struct query_columns_base: object_columns_base, virtual context
  {
    typedef query_columns_base base;

    query_columns_base (semantics::class_&, bool decl);

    virtual void
    traverse_object (semantics::class_&);

    virtual void
    traverse_composite (semantics::data_member*, semantics::class_&);

    virtual void
    traverse_pointer (semantics::data_member&, semantics::class_&);

  protected:
    bool decl_;
    string scope_;
    string tag_scope_;
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
    column_ctor (string const& type, string const& base);

    virtual void
    column_ctor_extra (semantics::data_member&)
    {
    }

    virtual void
    traverse_object (semantics::class_&);

    virtual void
    traverse_composite (semantics::data_member*, semantics::class_&);

    virtual void
    column_common (semantics::data_member&,
                   string const& type,
                   string const& column,
                   string const& suffix = "_type_");

    virtual bool
    traverse_column (semantics::data_member&, string const&, bool);

    virtual void
    traverse_pointer (semantics::data_member&, semantics::class_&);

  protected:
    bool ptr_;
    bool decl_;

    bool in_ptr_; // True if we are "inside" an object pointer.

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
      database db (context::current ().options.database ()[0]);

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

#include <odb/relational/common.txx>

#endif // ODB_RELATIONAL_COMMON_HXX
