// file      : odb/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/generate.hxx>

using namespace std;

namespace
{
  struct data_member: traversal::data_member, context
  {
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
    class_ ()
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

namespace header
{
  void
  generate ()
  {
    context ctx;
    ostream& os (ctx.os);

    os << "#include <memory>" << endl
       << "#include <cstddef>" << endl // std::size_t
       << endl;

    os << "#include <odb/core.hxx>" << endl
       << "#include <odb/traits.hxx>" << endl
       << "#include <odb/callback.hxx>" << endl
       << "#include <odb/pointer-traits.hxx>" << endl;

    // In case of a boost TR1 implementation, we cannot distinguish
    // between the boost::shared_ptr and std::tr1::shared_ptr usage since
    // the latter is just a using-declaration for the former. To resolve
    // this we will include TR1 traits if the Boost TR1 header is included.
    //
    if (ctx.unit.count ("tr1-pointer-used") &&
        ctx.unit.get<bool> ("tr1-pointer-used"))
    {
      os << "#include <odb/tr1/pointer-traits.hxx>" << endl;
    }
    else if (ctx.unit.count ("boost-pointer-used") &&
             ctx.unit.get<bool> ("boost-pointer-used"))
    {
      os << "#ifdef BOOST_TR1_MEMORY_HPP_INCLUDED" << endl
         << "#  include <odb/tr1/pointer-traits.hxx>" << endl
         << "#endif" << endl;
    }

    os << "#include <odb/container-traits.hxx>" << endl;

    if (ctx.options.generate_query ())
      os << "#include <odb/result.hxx>" << endl;

    os << endl;

    /*
      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::namespace_ ns;
      class_ c;

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;

      os << "namespace odb"
         << "{";

      unit.dispatch (ctx.unit);

      os << "}";
    */
  }
}
