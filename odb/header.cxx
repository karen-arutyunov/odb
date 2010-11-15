// file      : odb/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/header.hxx>

namespace
{
  struct data_member: traversal::data_member, context
  {
    data_member (context& c)
        : context (c)
    {
    }

    virtual void
    traverse (semantics::data_member& m)
    {
      if (m.count ("transient"))
        return;

      string const& name (public_name (m));
      string const& type (m.type ().fq_name (m.belongs ().hint ()));

      os << "static " << type << "&" << endl
         << name << " (value_type&);"
         << endl;

      os << "static const " << type << "&" << endl
         << name << " (const value_type&);"
         << endl;
    }
  };

  struct class_: traversal::class_, context
  {
    class_ (context& c)
        : context (c), member_ (c)
    {
      member_names_ >> member_;
    }

    virtual void
    traverse (type& c)
    {
      if (c.file () != unit.file ())
        return;

      if (!comp_value (c))
        return;

      string const& type (c.fq_name ());

      os << "// " << c.name () << endl
         << "//" << endl;

      os << "template <>" << endl
         << "class access::value_traits< " << type << " >"
         << "{"
         << "public:" << endl;

      // value_type
      //
      os << "typedef " << type << " value_type;"
         << endl;

      names (c, member_names_);

      os << "};";
    }

  private:
    data_member member_;
    traversal::names member_names_;
  };
}

void
generate_header (context& ctx)
{
  ctx.os << "#include <memory>" << endl
         << endl;

  ctx.os << "#include <odb/core.hxx>" << endl
         << "#include <odb/traits.hxx>" << endl
         << endl;

  /*
  traversal::unit unit;
  traversal::defines unit_defines;
  traversal::namespace_ ns;
  class_ c (ctx);

  unit >> unit_defines >> ns;
  unit_defines >> c;

  traversal::defines ns_defines;

  ns >> ns_defines >> ns;
  ns_defines >> c;

  ctx.os << "namespace odb"
         << "{";

  unit.dispatch (ctx.unit);

  ctx.os << "}";
  */
}
