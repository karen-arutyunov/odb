// file      : odb/sqlite/query-const-expr.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/sqlite/query.hxx>

namespace odb
{
  namespace sqlite
  {
    // Sun CC cannot handle this in query.cxx.
    //
    const query query::true_expr (true);
  }
}
