// file      : odb/semantics/fundamental.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_FUNDAMENTAL_HXX
#define ODB_SEMANTICS_FUNDAMENTAL_HXX

#include <semantics/elements.hxx>

namespace semantics
{
  //
  // Fundamental C++ types.
  //

  class fund_type: public type {};

  struct fund_void: fund_type
  {
    fund_void (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_bool: fund_type
  {
    fund_bool (): node (path ("<fundamental>"), 0, 0) {}
  };

  //
  // Integral.
  //

  struct fund_char: fund_type
  {
    fund_char (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_wchar: fund_type
  {
    fund_wchar (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_signed_char: fund_type
  {
    fund_signed_char (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_unsigned_char: fund_type
  {
    fund_unsigned_char (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_short: fund_type
  {
    fund_short (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_unsigned_short: fund_type
  {
    fund_unsigned_short (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_int: fund_type
  {
    fund_int (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_unsigned_int: fund_type
  {
    fund_unsigned_int (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_long: fund_type
  {
    fund_long (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_unsigned_long: fund_type
  {
    fund_unsigned_long (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_long_long: fund_type
  {
    fund_long_long (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_unsigned_long_long: fund_type
  {
    fund_unsigned_long_long (): node (path ("<fundamental>"), 0, 0) {}
  };

  //
  // Real.
  //

  struct fund_float: fund_type
  {
    fund_float (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_double: fund_type
  {
    fund_double (): node (path ("<fundamental>"), 0, 0) {}
  };

  struct fund_long_double: fund_type
  {
    fund_long_double (): node (path ("<fundamental>"), 0, 0) {}
  };
}

#endif // ODB_SEMANTICS_FUNDAMENTAL_HXX
