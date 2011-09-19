// file      : odb/validator.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_VALIDATOR_HXX
#define ODB_VALIDATOR_HXX

#include <odb/options.hxx>
#include <odb/semantics/unit.hxx>

class validator
{
public:
  // The first pass is performed before processing. The second -- after.
  //
  bool
  validate (options const&,
            semantics::unit&,
            semantics::path const&,
            unsigned short pass);

  validator () {}

private:
  validator (validator const&);
  validator& operator= (validator const&);
};

#endif // ODB_VALIDATOR_HXX
