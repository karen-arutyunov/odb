// file      : odb/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <stack>
#include "context.hxx"

using namespace std;

context::
context (ostream& os_,
         semantics::unit& unit_,
         options_type const& ops)
    : data_ (new (shared) data),
      os (os_),
      unit (unit_),
      options (ops)
{
}

context::
context (context& c)
    : data_ (c.data_),
      os (c.os),
      unit (c.unit),
      options (c.options)
{
}

// namespace
//

void namespace_::
traverse (type& ns)
{
  string name (ns.name ());

  if (name.empty ())
    os << "namespace";
  else
    os << "namespace " << name;

  os << "{";

  traversal::namespace_::traverse (ns);

  os << "}";
}
