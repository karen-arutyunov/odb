// file      : odb/semantics/elements.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/elements.hxx>

namespace semantics
{
  // access
  //
  static char const* access_str[] = {"public", "protected", "private"};

  char const* access::
  string () const
  {
    return access_str[value_];
  }

  //
  //
  node::
  node (path const& file, size_t line, size_t column)
      : file_ (file), line_ (line), column_ (column)
  {
  }

  node::
  node ()
      : file_ ("")
  {
    // GCC plugin machinery #define's abort as a macro.
    //
    // std::abort ();
    abort ();
  }

  // nameable
  //

  bool nameable::
  fq_anonymous () const
  {
    // Nameable is fq-anonymous if all the paths to the global scope
    // have at least one anonymous link.
    //
    if (anonymous ())
      return true;

    if (named ().global_scope ())
      return false;

    if (defined_ != 0 && !defined_->scope ().fq_anonymous ())
      return false;

    for (names_list::const_iterator i (named_.begin ()), e (named_.end ());
         i != e; ++i)
    {
      if (!(*i)->scope ().fq_anonymous ())
        return false;
    }

    return true;
  }

  bool nameable::
  fq_anonymous (names* hint) const
  {
    if (hint == 0 && defined_ == 0)
      return true;

    names& n (hint ? *hint : *defined_);

    if (n.global_scope ())
      return false;

    return n.scope ().fq_anonymous ();
  }

  string nameable::
  fq_name () const
  {
    if (named ().global_scope ())
      return "";

    if (defined_ != 0 && !defined_->scope ().fq_anonymous ())
      return defined_->scope ().fq_name () + "::" + name ();

    for (names_list::const_iterator i (named_.begin ()), e (named_.end ());
         i != e; ++i)
    {
      if (!(*i)->scope ().fq_anonymous ())
        return (*i)->scope ().fq_name () + "::" + name ();
    }

    return "<anonymous>";
  }

  string nameable::
  fq_name (names* hint) const
  {
    names& n (hint ? *hint : *defined_);

    if (n.global_scope ())
      return "";

    return n.scope ().fq_name () + "::" + n.name ();
  }

  // scope
  //

  scope::names_iterator_pair scope::
  find (string const& name) const
  {
    names_map::const_iterator i (names_map_.find (name));

    if (i == names_map_.end ())
      return names_iterator_pair (names_.end (), names_.end ());
    else
      return names_iterator_pair (i->second.begin (), i->second.end ());
  }

  scope::names_iterator scope::
  find (names& e)
  {
    list_iterator_map::iterator i (iterator_map_.find (&e));
    return i != iterator_map_.end () ? i->second : names_.end ();
  }

  void scope::
  add_edge_left (names& e)
  {
    names_list::iterator it (names_.insert (names_.end (), &e));
    iterator_map_[&e] = it;
    names_map_[e.name ()].push_back (&e);
  }

  // type info
  //
  namespace
  {
    struct init
    {
      init ()
      {
        using compiler::type_info;

        // node
        //
        insert (type_info (typeid (node)));

        // edge
        //
        insert (type_info (typeid (edge)));

        // names
        //
        {
          type_info ti (typeid (names));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // declares
        //
        {
          type_info ti (typeid (declares));
          ti.add_base (typeid (names));
          insert (ti);
        }

        // defines
        //
        {
          type_info ti (typeid (defines));
          ti.add_base (typeid (declares));
          insert (ti);
        }

        // typedefs
        //
        {
          type_info ti (typeid (typedefs));
          ti.add_base (typeid (declares));
          insert (ti);
        }

        // nameable
        //
        {
          type_info ti (typeid (nameable));
          ti.add_base (typeid (node));
          insert (ti);
        }

        // scope
        //
        {
          type_info ti (typeid (scope));
          ti.add_base (typeid (nameable));
          insert (ti);
        }

        // type
        //
        {
          type_info ti (typeid (type));
          ti.add_base (typeid (nameable));
          insert (ti);
        }

        // belongs
        //
        {
          type_info ti (typeid (belongs));
          ti.add_base (typeid (edge));
          insert (ti);
        }

        // instance
        //
        {
          type_info ti (typeid (instance));
          ti.add_base (typeid (node));
          insert (ti);
        }

        // data_member
        //
        {
          type_info ti (typeid (data_member));
          ti.add_base (typeid (nameable));
          ti.add_base (typeid (instance));
          insert (ti);
        }

        // unsupported_type
        //
        {
          type_info ti (typeid (unsupported_type));
          ti.add_base (typeid (type));
          insert (ti);
        }
      }
    } init_;
  }
}