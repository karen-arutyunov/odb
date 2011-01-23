// file      : odb/semantics/elements.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <cutl/compiler/type-info.hxx>

#include <odb/cxx-lexer.hxx>

#include <odb/semantics/elements.hxx>
#include <odb/semantics/unit.hxx>

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
  node (path const& file, size_t line, size_t column, tree tn)
      : tree_node_ (tn), file_ (file), line_ (line), column_ (column)
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
  anonymous_ () const
  {
    tree n (tree_node ());

    if (TYPE_P (n))
    {
      tree name (0);

      if (tree decl = TYPE_NAME (n))
        name = DECL_NAME (decl);

      return name != 0 && ANON_AGGRNAME_P (name);
    }

    return true;
  }

  bool nameable::
  fq_anonymous () const
  {
    // Nameable is fq-anonymous if all the paths to the global scope
    // have at least one anonymous link.
    //
    if (defined_ != 0 || !named_.empty ())
    {
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
    }
    else
    {
      // If we can get a literal name for this type, then it is not
      // anonymous as long as its scope is not anonymous.
      //
      tree type (tree_node ());

      if (TYPE_P (type))
      {
        tree name (0);

        if (tree decl = TYPE_NAME (type))
        {
          name = DECL_NAME (decl);
          if (name != 0 && ANON_AGGRNAME_P (name))
            return true;

          tree s (CP_DECL_CONTEXT (decl));

          if (TREE_CODE (s) == TYPE_DECL)
            s = TREE_TYPE (s);

          if (nameable* n = dynamic_cast<nameable*> (unit ().find (s)))
            return n->fq_anonymous ();
        }
        else
          return false; // Assume this is a derived type (e.g., pointer).
      }
    }

    return true;
  }

  bool nameable::
  fq_anonymous (names* hint) const
  {
    if (hint != 0 || defined_ != 0)
    {
      names& n (hint ? *hint : *defined_);

      if (n.global_scope ())
        return false;

      return n.scope ().fq_anonymous ();
    }
    else
      return fq_anonymous ();
  }

  static string
  qualify_names (string const& n, bool qualify_first)
  {
    // @@ Creating a lexer for each call is a bad idea. Need
    //    to cache it somewhere.
    //
    cxx_lexer l;
    l.start (n);

    string r, t;
    bool punc (false);
    bool scoped (false);

    for (cpp_ttype tt = l.next (t); tt != CPP_EOF; tt = l.next (t))
    {
      if (punc && tt > CPP_LAST_PUNCTUATOR)
        r += ' ';

      punc = false;

      switch (static_cast<unsigned> (tt))
      {
      case CPP_LESS:
        {
          r += "< ";
          break;
        }
      case CPP_GREATER:
        {
          r += " >";
          break;
        }
      case CPP_COMMA:
        {
          r += ", ";
          break;
        }
      case CPP_NAME:
        {
          // If the name was not preceeded with '::', qualify it.
          //
          if (!scoped)
          {
            if (!qualify_first)
              qualify_first = true;
            else
              r += "::";
          }

          r += t;
          punc = true;
          break;
        }
      case CPP_KEYWORD:
      case CPP_NUMBER:
        {
          r += t;
          punc = true;
          break;
        }
      default:
        {
          r += t;
          break;
        }
      }

      scoped = (tt == CPP_SCOPE);
    }

    return r;
  }

  string nameable::
  name_ () const
  {
    tree n (tree_node ());

    if (!TYPE_P (n))
      return "<anonymous>";

    // @@ Doing this once and caching the result is probably a
    //    good idea.
    //
    return qualify_names (
      type_as_string (n, TFF_PLAIN_IDENTIFIER | TFF_UNQUALIFIED_NAME), false);
  }

  string nameable::
  fq_name () const
  {
    // @@ Doing this once and caching the result is probably a
    //    good idea.
    //

    if (named_p () && named ().global_scope ())
      return "";

    if (defined_ != 0 && !defined_->scope ().fq_anonymous ())
      return defined_->scope ().fq_name () + "::" + name ();

    for (names_list::const_iterator i (named_.begin ()), e (named_.end ());
         i != e; ++i)
    {
      if (!(*i)->scope ().fq_anonymous ())
        return (*i)->scope ().fq_name () + "::" + name ();
    }

    tree n (tree_node ());

    if (!TYPE_P (n))
      return "<anonymous>";

    return qualify_names (type_as_string (n, TFF_PLAIN_IDENTIFIER), true);
  }

  string nameable::
  fq_name (names* hint) const
  {
    if (hint != 0 || defined_ != 0)
    {
      names& n (hint ? *hint : *defined_);

      if (n.global_scope ())
        return "";

      return n.scope ().fq_name () + "::" + n.name ();
    }
    else
    {
      // Since there was no hint, prefer the literal name over the names
      // edges.
      //
      tree n (tree_node ());

      if (TYPE_P (n))
        return qualify_names (type_as_string (n, TFF_PLAIN_IDENTIFIER), true);

      // Last resort is to call the other version of fq_name which will
      // check the names edges.
      //
      return fq_name ();
    }
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