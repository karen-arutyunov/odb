// file      : odb/inline.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/inline.hxx>

namespace
{
  struct data_member: traversal::data_member, context
  {
    data_member (context& c, semantics::class_& cl)
        : context (c)
    {
      scope_ = "access::value_traits< " + cl.fq_name () + " >";
    }

    virtual void
    traverse (semantics::data_member& m)
    {
      if (m.count ("transient"))
        return;

      string const& name (public_name (m));
      string const& type (m.type ().fq_name (m.belongs ().hint ()));

      os << "inline" << endl
         << type << "& " << scope_ << "::" << endl
         << name << " (value_type& v)"
         << "{"
         << "return v." << m.name () << ";"
         << "}";

      os << "inline" << endl
         << "const " << type << "& " << scope_ << "::" << endl
         << name << " (const value_type& v)"
         << "{"
         << "return v." << m.name () << ";"
         << "}";
    }

  private:
    string scope_;
  };

  struct class_: traversal::class_, context
  {
    class_ (context& c)
        : context (c)
    {
    }

    virtual void
    traverse (type& c)
    {
      if (c.file () != unit.file ())
        return;

      if (!comp_value (c))
        return;

      os << "// " << c.name () << endl
         << "//" << endl;

      data_member member (*this, c);
      traversal::names member_names (member);
      names (c, member_names);
    }
  };
}

void
generate_inline (context& /*ctx*/)
{
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
