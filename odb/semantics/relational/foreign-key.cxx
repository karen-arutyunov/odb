// file      : odb/semantics/relational/foreign-key.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <ostream>
#include <istream>

#include <cutl/compiler/type-info.hxx>
#include <odb/semantics/relational.hxx>

using namespace std;

namespace semantics
{
  namespace relational
  {
    static const char* action_str[] = {"NO ACTION", "CASCADE"};

    ostream&
    operator<< (ostream& os, foreign_key::action_type v)
    {
      return os << action_str[v];
    }

    istream&
    operator>> (istream& is, foreign_key::action_type& v)
    {
      string s;
      getline (is, s);

      if (!is.eof ())
        is.setstate (istream::failbit);

      if (!is.fail ())
      {
        if (s == "NO ACTION")
          v = foreign_key::no_action;
        else if (s == "CASCADE")
          v = foreign_key::cascade;
        else
          is.setstate (istream::failbit);
      }

      return is;
    }

    foreign_key::
    foreign_key (xml::parser& p, uscope& s, graph& g)
        : key (p, s, g),
          deferred_ (p.attribute ("deferred", false)),
          on_delete_ (p.attribute ("on-delete", no_action))
    {
      using namespace xml;

      p.next_expect (parser::start_element, xmlns, "references");
      referenced_table_ = p.attribute<qname> ("table");
      p.content (parser::complex);

      for (parser::event_type e (p.peek ());
           e == parser::start_element;
           e = p.peek ())
      {
        if (p.qname () != xml::qname (xmlns, "column"))
          break; // Not our elements.

        p.next ();
        referenced_columns_.push_back (p.attribute<uname> ("name"));
        p.content (parser::empty);
        p.next_expect (parser::end_element);
      }

      p.next_expect (parser::end_element);
    }

    void foreign_key::
    serialize (xml::serializer& s) const
    {
      s.start_element (xmlns, "foreign-key");
      key::serialize_attributes (s);

      if (deferred ())
        s.attribute ("deferred", true);

      if (on_delete () != no_action)
        s.attribute ("on-delete", on_delete ());

      key::serialize_content (s);

      // Referenced columns.
      //
      s.start_element (xmlns, "references");
      s.attribute ("table", referenced_table ());

      for (columns::const_iterator i (referenced_columns_.begin ());
           i != referenced_columns_.end (); ++i)
      {
        s.start_element (xmlns, "column");
        s.attribute ("name", *i);
        s.end_element ();
      }

      s.end_element (); // references
      s.end_element (); // foreign-key
    }

    // type info
    //
    namespace
    {
      struct init
      {
        init ()
        {
          unameable::parser_map_["foreign-key"] =
            &unameable::parser_impl<foreign_key>;

          using compiler::type_info;

          {
            type_info ti (typeid (foreign_key));
            ti.add_base (typeid (key));
            insert (ti);
          }
        }
      } init_;
    }
  }
}
