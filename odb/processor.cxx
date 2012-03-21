// file      : odb/processor.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <iostream>

#include <odb/context.hxx>
#include <odb/processor.hxx>

#include <odb/relational/processor.hxx>

using namespace std;

void processor::
process (options const& ops,
         features& f,
         semantics::unit& unit,
         semantics::path const&)
{
  try
  {
    // Process types.
    //
    auto_ptr<context> ctx (create_context (cerr, unit, ops, f, 0));
    relational::process ();
  }
  catch (operation_failed const&)
  {
    // Processing failed. Diagnostics has already been issued.
    //
    throw failed ();
  }
}
