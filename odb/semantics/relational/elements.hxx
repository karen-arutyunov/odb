// file      : odb/semantics/relational/elements.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_ELEMENTS_HXX
#define ODB_SEMANTICS_RELATIONAL_ELEMENTS_HXX

#include <map>
#include <list>
#include <vector>
#include <string>
#include <cassert>

#include <cutl/container/graph.hxx>
#include <cutl/container/pointer-iterator.hxx>
#include <cutl/compiler/context.hxx>

namespace semantics
{
  namespace relational
  {
    using namespace cutl;

    using std::string;

    using container::graph;
    using container::pointer_iterator;

    using compiler::context;

    //
    //
    class node;
    class edge;

    //
    //
    class edge: public context
    {
    public:
      virtual
      ~edge () {}

    public:
      template <typename X>
      bool
      is_a () const
      {
        return dynamic_cast<X const*> (this) != 0;
      }
    };

    //
    //
    class node: public context
    {
    public:
      virtual
      ~node () {}

      // Return name of the node.
      //
      virtual string
      kind () const = 0;

    public:
      template <typename X>
      bool
      is_a () const
      {
        return dynamic_cast<X const*> (this) != 0;
      }

      // Sink functions that allow extensions in the form of one-way
      // edges.
      //
    public:
      void
      add_edge_right (edge&)
      {
      }
    };

    //
    //
    class scope;
    class nameable;

    //
    //
    class names: public edge
    {
    public:
      typedef relational::scope scope_type;
      typedef relational::nameable nameable_type;

      string const&
      name () const
      {
        return name_;
      }

      scope_type&
      scope () const
      {
        return *scope_;
      }

      nameable_type&
      nameable () const
      {
        return *nameable_;
      }

    public:
      names (string const& name): name_ (name) {}

      void
      set_left_node (scope_type& n)
      {
        scope_ = &n;
      }

      void
      set_right_node (nameable_type& n)
      {
        nameable_ = &n;
      }

    protected:
      string name_;
      scope_type* scope_;
      nameable_type* nameable_;
    };

    //
    //
    class nameable: public virtual node
    {
    public:
      typedef relational::scope scope_type;

      string const&
      name () const
      {
        return named_->name ();
      }

      scope_type&
      scope () const
      {
        return named ().scope ();
      }

      names&
      named () const
      {
        return *named_;
      }

    public:
      // Id identifies the C++ node (e.g., a class or a data member) that
      // this model node corresponds to. The ids are not necessarily unique
      // (e.g., there can be a table and an index with the same id that
      // correspond to a container member). However, in any given scope,
      // the {id,typeid} must be unique. This becomes important when we
      // try to find correspondance between nodes during model diff'ing.
      //
      nameable (string const& id): id_ (id), named_ (0) {}

      void
      add_edge_right (names& e)
      {
        assert (named_ == 0);
        named_ = &e;
      }

      using node::add_edge_right;

    private:
      string id_;
      names* named_;
    };


    //
    //
    struct duplicate_name
    {
      typedef relational::scope scope_type;
      typedef relational::nameable nameable_type;

      duplicate_name (scope_type& s, nameable_type& n, nameable_type& d)
          : scope (s), nameable (n), duplicate (d)
      {
      }

      scope_type& scope;
      nameable_type& nameable;
      nameable_type& duplicate;
    };

    class scope: public virtual node
    {
    protected:
      typedef std::list<names*> names_list;
      typedef std::map<string, names_list::iterator> names_map;
      typedef std::map<names const*, names_list::iterator> names_iterator_map;

    public:
      typedef pointer_iterator<names_list::iterator> names_iterator;
      typedef
      pointer_iterator<names_list::const_iterator>
      names_const_iterator;

    public:
      // Iteration.
      //
      names_iterator
      names_begin ()
      {
        return names_.begin ();
      }

      names_iterator
      names_end ()
      {
        return names_.end ();
      }

      names_const_iterator
      names_begin () const
      {
        return names_.begin ();
      }

      names_const_iterator
      names_end () const
      {
        return names_.end ();
      }

      // Find.
      //
      names_iterator
      find (string const& name);

      names_const_iterator
      find (string const& name) const;

      names_iterator
      find (names const&);

      names_const_iterator
      find (names const&) const;

    public:
      scope ()
          : first_key_ (names_.end ())
      {
      }

      void
      add_edge_left (names&);

    private:
      names_list names_;
      names_map names_map_;
      names_iterator_map iterator_map_;

      names_list::iterator first_key_;
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_ELEMENTS_HXX
