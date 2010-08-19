// file      : odb/validator.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_VALIDATOR_HXX
#define ODB_VALIDATOR_HXX

#include <odb/options.hxx>
#include <odb/semantics/unit.hxx>

class validator
{
public:
  validator ();

  bool
  validate (options const&, semantics::unit&, semantics::path const&);

private:
  validator (validator const&);
  validator& operator= (validator const&);
};

#endif // ODB_VALIDATOR_HXX
