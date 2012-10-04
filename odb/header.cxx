// file      : odb/header.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/generate.hxx>

using namespace std;

namespace header
{
  void
  generate ()
  {
    context ctx;
    ostream& os (ctx.os);

    os << "#include <memory>" << endl
       << "#include <cstddef>" << endl; // std::size_t

    if (ctx.features.polymorphic_object)
      os << "#include <string>" << endl; // For discriminator.

    os << endl;

    os << "#include <odb/core.hxx>" << endl
       << "#include <odb/traits.hxx>" << endl
       << "#include <odb/callback.hxx>" << endl
       << "#include <odb/wrapper-traits.hxx>" << endl
       << "#include <odb/pointer-traits.hxx>" << endl;

    // In case of a boost TR1 implementation, we cannot distinguish
    // between the boost::shared_ptr and std::tr1::shared_ptr usage since
    // the latter is just a using-declaration for the former. To resolve
    // this we will include TR1 traits if the Boost TR1 header is included.
    //
    if (ctx.features.tr1_pointer)
    {
      os << "#include <odb/tr1/wrapper-traits.hxx>" << endl
         << "#include <odb/tr1/pointer-traits.hxx>" << endl;
    }
    else if (ctx.features.boost_pointer)
    {
      os << "#ifdef BOOST_TR1_MEMORY_HPP_INCLUDED" << endl
         << "#  include <odb/tr1/wrapper-traits.hxx>" << endl
         << "#  include <odb/tr1/pointer-traits.hxx>" << endl
         << "#endif" << endl;
    }

    os << "#include <odb/container-traits.hxx>" << endl;

    if (ctx.features.session_object)
      os << "#include <odb/cache-traits.hxx>" << endl;
    else
      os << "#include <odb/no-op-cache-traits.hxx>" << endl;

    if (ctx.features.polymorphic_object)
      os << "#include <odb/polymorphic-info.hxx>" << endl;

    if (ctx.options.generate_query ())
    {
      if (!ctx.options.omit_prepared ())
        os << "#include <odb/prepared-query.hxx>" << endl;

      os << "#include <odb/result.hxx>" << endl;

      if (ctx.features.simple_object)
        os << "#include <odb/simple-object-result.hxx>" << endl;

      if (ctx.features.polymorphic_object)
        os << "#include <odb/polymorphic-object-result.hxx>" << endl;

      if (ctx.features.no_id_object)
        os << "#include <odb/no-id-object-result.hxx>" << endl;

      if (ctx.features.view)
        os << "#include <odb/view-result.hxx>" << endl;
    }

    os << endl;
  }
}
