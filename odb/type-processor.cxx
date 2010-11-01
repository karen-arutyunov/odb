// file      : odb/type-processor.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/type-processor.hxx>

namespace
{
  struct data_member: traversal::data_member, context
  {
    data_member (context& c)
        : context (c)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (m.count ("transient"))
        return;

      semantics::type& t (m.type ());

      // Nothing to do if this is a composite value type.
      //
      if (comp_value (t))
        return;

      string const& type (column_type_impl (m));

      if (!type.empty ())
      {
        m.set ("column-type", type);
        return;
      }

      // See if this is a container type.
      //

      // If it is none of the above then we have an error.
      //
      string const& fq_type (t.fq_name (m.belongs ().hint ()));

      cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
           << " error: unable to map C++ type '" << fq_type << "' used in "
           << "data member '" << m.name () << "' to a database type" << endl;

      cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
           << " info: use '#pragma db type' to specify the database type"
           << endl;

      throw generation_failed ();
    }
  };

  struct class_: traversal::class_, context
  {
    class_ (context& c)
        : context (c), member_ (c)
    {
      *this >> member_names_ >> member_;
    }

    virtual void
    traverse (type& c)
    {
      if (!(c.count ("object") || comp_value (c)))
        return;

      names (c);
    }

  private:
    data_member member_;
    traversal::names member_names_;
  };
}

void
process_types (context& ctx)
{
  traversal::unit unit;
  traversal::defines unit_defines;
  traversal::namespace_ ns;
  class_ c (ctx);

  unit >> unit_defines >> ns;
  unit_defines >> c;

  traversal::defines ns_defines;

  ns >> ns_defines >> ns;
  ns_defines >> c;

  unit.dispatch (ctx.unit);
}
