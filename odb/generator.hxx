// file      : odb/generator.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_GENERATOR_HXX
#define ODB_GENERATOR_HXX

#include <odb/options.hxx>
#include <odb/semantics/unit.hxx>

class generator
{
public:
  generator ();

  class failed {};

  void
  generate (options const&, semantics::unit&, semantics::path const&);

private:
  generator (generator const&);

  generator&
  operator= (generator const&);
};

#endif // ODB_GENERATOR_HXX
