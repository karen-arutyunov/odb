// file      : odb/semantics/class-template.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_CLASS_TEMPLATE_HXX
#define ODB_SEMANTICS_CLASS_TEMPLATE_HXX

#include <odb/semantics/elements.hxx>
#include <odb/semantics/class.hxx>
#include <odb/semantics/template.hxx>

namespace semantics
{
  class class_template: public type_template, public scope
  {
  private:
    typedef std::vector<inherits*> inherits_list;

  public:
    typedef pointer_iterator<inherits_list::const_iterator> inherits_iterator;

    inherits_iterator
    inherits_begin () const
    {
      return inherits_.begin ();
    }

    inherits_iterator
    inherits_end () const
    {
      return inherits_.end ();
    }

  public:
    class_template (path const& file, size_t line, size_t column)
        : node (file, line, column)
    {
    }

    void
    add_edge_left (inherits& e)
    {
      inherits_.push_back (&e);
    }

    void
    add_edge_right (inherits&)
    {
    }

    using scope::add_edge_left;
    using scope::add_edge_right;

    // Resolve conflict between scope::scope and nameable::scope.
    //
    using nameable::scope;

  private:
    inherits_list inherits_;
  };

  class class_instantiation: public class_, public type_instantiation
  {
  public:
    class_instantiation (path const& file,
                         size_t line,
                         size_t column,
                         tree tn)
        : node (file, line, column), type (tn)
    {
    }

    using class_::add_edge_left;
    using type_instantiation::add_edge_left;
  };
}

#endif // ODB_SEMANTICS_CLASS_TEMPLATE_HXX
