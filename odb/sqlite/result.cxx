// file      : odb/sqlite/result.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/sqlite/result.hxx>
#include <odb/sqlite/query.hxx>

namespace odb
{
  namespace sqlite
  {
    result_impl_base::
    result_impl_base (const query& q, details::shared_ptr<select_statement> s)
        : params_ (q.parameters ()), statement_ (s)
    {
    }
  }
}
