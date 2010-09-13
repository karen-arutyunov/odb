// file      : odb/semantics/fundamental.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_FUNDAMENTAL_HXX
#define ODB_SEMANTICS_FUNDAMENTAL_HXX

#include <odb/semantics/elements.hxx>

namespace semantics
{
  //
  // Fundamental C++ types.
  //

  struct fund_type: public type
  {
    virtual string
    fq_name () const;

    virtual string
    fq_name (names*) const;

  protected:
    fund_type (tree tn);
  };

  struct fund_void: fund_type
  {
    fund_void (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_bool: fund_type
  {
    fund_bool (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  //
  // Integral.
  //

  struct fund_char: fund_type
  {
    fund_char (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_wchar: fund_type
  {
    fund_wchar (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_signed_char: fund_type
  {
    fund_signed_char (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_unsigned_char: fund_type
  {
    fund_unsigned_char (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_short: fund_type
  {
    fund_short (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_unsigned_short: fund_type
  {
    fund_unsigned_short (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_int: fund_type
  {
    fund_int (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_unsigned_int: fund_type
  {
    fund_unsigned_int (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_long: fund_type
  {
    fund_long (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_unsigned_long: fund_type
  {
    fund_unsigned_long (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_long_long: fund_type
  {
    fund_long_long (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_unsigned_long_long: fund_type
  {
    fund_unsigned_long_long (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  //
  // Real.
  //

  struct fund_float: fund_type
  {
    fund_float (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_double: fund_type
  {
    fund_double (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };

  struct fund_long_double: fund_type
  {
    fund_long_double (tree tn)
        : node (path ("<fundamental>"), 0, 0), fund_type (tn) {}
  };
}

#endif // ODB_SEMANTICS_FUNDAMENTAL_HXX
