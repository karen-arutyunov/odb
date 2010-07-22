// file      : odb/sql-token.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQL_TOKEN_HXX
#define ODB_SQL_TOKEN_HXX

#include <string>
#include <cstddef> // std::size_t

class sql_token
{
public:
  enum token_type
  {
    t_eos,
    t_identifier,
    t_punctuation,
    t_string_lit,
    t_int_lit,
    t_float_lit,
  };

  token_type
  type () const;

  // Identifier
  //
public:
  std::string const&
  identifier () const;

  // Punctuation
  //
public:
  enum punctuation_type
  {
    p_semi,
    p_comma,
    p_lparen,
    p_rparen,
    p_eq,
    p_invalid
  };

  // Return the punctuation id if type is t_punctuation and p_invalid
  // otherwise.
  //
  punctuation_type
  punctuation () const;

  // Literals.
  //
public:
  std::string const&
  literal () const;

  // C-tors.
  //
public:
  // EOS and punctuations.
  //
  sql_token ();
  sql_token (punctuation_type p);

  // Identifier and literals.
  //
  sql_token (token_type t, std::string const& s);

private:
  token_type type_;
  punctuation_type punctuation_;
  std::string str_;
};

#include <odb/sql-token.ixx>

#endif // ODB_SQL_TOKEN_HXX
