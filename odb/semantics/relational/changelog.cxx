// file      : odb/semantics/relational/changelog.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <vector>

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

namespace semantics
{
  namespace relational
  {
    changelog::
    changelog (xml::parser& p)
        : contains_model_ (0)
    {
      using namespace xml;

      p.next_expect (parser::start_element, xmlns, "changelog");
      p.content (parser::complex);

      if (p.attribute<unsigned int> ("version") != 1)
        throw parsing (p, "unsupported changelog format version");

      // Get the changesets. Because they are stored in the reverse order,
      // first save them to the temporary vector.
      //
      typedef std::vector<changeset*> changesets;
      changesets cs;

      for (parser::event_type e (p.peek ());
           e == parser::start_element;
           e = p.peek ())
      {
        if (p.qname () != xml::qname (xmlns, "changeset"))
          break; // Not our elements.

        p.next ();
        cs.push_back (&new_node<changeset> (p, *this));
        p.next_expect (parser::end_element);
      }

      for (changesets::reverse_iterator i (cs.rbegin ()); i != cs.rend (); ++i)
        new_edge<contains_changeset> (*this, **i);

      // Get the model.
      //
      p.next_expect (parser::start_element, xmlns, "model");

      model_type& m (new_node<model_type> (p, *this));
      new_edge<contains_model_type> (*this, m);

      p.next_expect (parser::end_element);
      p.next_expect (parser::end_element);
    }

    void changelog::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "changelog");
      s.namespace_decl (xmlns, "");
      s.attribute ("version", 1); // Format version.

      // For better readability serialize things in reverse order so that
      // the most recent changeset appears first.
      //
      for (contains_changeset_list::const_reverse_iterator i (
             contains_changeset_.rbegin ());
           i != contains_changeset_.rend (); ++i)
      {
        (*i)->changeset ().serialize (s);
      }

      model ().serialize (s);
      s.end_element ();
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

          // contains_model
          //
          {
            type_info ti (typeid (contains_model));
            ti.add_base (typeid (edge));
            insert (ti);
          }

          // contains_changeset
          //
          {
            type_info ti (typeid (contains_changeset));
            ti.add_base (typeid (edge));
            insert (ti);
          }

          // changelog
          //
          {
            type_info ti (typeid (changelog));
            ti.add_base (typeid (node));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
