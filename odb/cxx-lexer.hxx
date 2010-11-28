// file      : odb/cxx-lexer.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_CXX_LEXER_HXX
#define ODB_CXX_LEXER_HXX

#include <odb/gcc.hxx>

#include <string>

// A C++ keyword. This is an extension to libcpp token types.
//
#define CPP_KEYWORD ((cpp_ttype) (N_TTYPES + 1))

// A thin wrapper around cpp_reader for lexing C++ code fragments.
//
class cxx_lexer
{
public:
  cxx_lexer ();
  ~cxx_lexer ();

public:
  struct invalid_input {};

  void
  start (std::string const&);

  cpp_ttype
  next (std::string& token);

public:
  static char const* token_spelling[N_TTYPES + 1];

private:
  std::string data_;
  std::string buf_;
  line_maps line_map_;
  cpp_reader* reader_;
  cpp_callbacks* callbacks_;
};

#endif // ODB_CXX_LEXER_HXX
