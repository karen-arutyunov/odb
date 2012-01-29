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
       << "#include <cstddef>" << endl // std::size_t
       << endl;

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
    if (ctx.unit.count ("tr1-pointer-used") &&
        ctx.unit.get<bool> ("tr1-pointer-used"))
    {
      os << "#include <odb/tr1/wrapper-traits.hxx>" << endl
         << "#include <odb/tr1/pointer-traits.hxx>" << endl;
    }
    else if (ctx.unit.count ("boost-pointer-used") &&
             ctx.unit.get<bool> ("boost-pointer-used"))
    {
      os << "#ifdef BOOST_TR1_MEMORY_HPP_INCLUDED" << endl
         << "#  include <odb/tr1/wrapper-traits.hxx>" << endl
         << "#  include <odb/tr1/pointer-traits.hxx>" << endl
         << "#endif" << endl;
    }

    os << "#include <odb/container-traits.hxx>" << endl;

    if (ctx.options.generate_query ())
      os << "#include <odb/result.hxx>" << endl;

    os << endl;
  }
}
