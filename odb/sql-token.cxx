// file      : odb/sql-token.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <iostream>

#include <odb/sql-token.hxx>

using namespace std;

static char punctuation_literals[] = {';', ',', '(', ')', '='};

ostream&
operator<< (ostream& os, sql_token const& t)
{
  switch (t.type ())
  {
  case sql_token::t_eos:
    {
      os << "<end-of-stream>";
      break;
    }
  case sql_token::t_identifier:
    {
      os << t.identifier ();
      break;
    }
  case sql_token::t_punctuation:
    {
      os << punctuation_literals[t.punctuation ()];
      break;
    }
  case sql_token::t_string_lit:
  case sql_token::t_int_lit:
  case sql_token::t_float_lit:
    {
      os << t.literal ();
      break;
    }
  }

  return os;
}
