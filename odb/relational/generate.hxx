// file      : odb/relational/generate.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_GENERATE_HXX
#define ODB_RELATIONAL_GENERATE_HXX

#include <cutl/shared-ptr.hxx>

#include <odb/semantics/relational/model.hxx>

namespace relational
{
  namespace header
  {
    void
    generate ();
  }

  namespace inline_
  {
    void
    generate ();
  }

  namespace source
  {
    void
    generate ();
  }

  namespace model
  {
    cutl::shared_ptr<semantics::relational::model>
    generate ();
  }

  namespace schema
  {
    void
    generate ();
  }
}

#endif // ODB_RELATIONAL_GENERATE_HXX
