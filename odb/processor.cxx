// file      : odb/processor.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <iostream>

#include <odb/context.hxx>
#include <odb/processor.hxx>

#include <odb/relational/processor.hxx>

using namespace std;

void processor::
process (options const& ops, semantics::unit& unit, semantics::path const&)
{
  try
  {
    // Process types.
    //
    if (ops.database () != database::tracer)
    {
      auto_ptr<context> ctx (create_context (cerr, unit, ops));
      relational::process ();
    }
  }
  catch (operation_failed const&)
  {
    // Processing failed. Diagnostics has already been issued.
    //
    throw failed ();
  }
}
