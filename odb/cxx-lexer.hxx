// file      : odb/cxx-lexer.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_CXX_LEXER_HXX
#define ODB_CXX_LEXER_HXX

#include <odb/gcc.hxx>

#include <string>

#include <odb/cxx-token.hxx>

// A C++ keyword. This is an extension to libcpp token types.
//
#define CPP_KEYWORD ((cpp_ttype) (N_TTYPES + 1))

class cxx_lexer
{
public:
  virtual
  ~cxx_lexer ();

public:
  struct invalid_input {};

  virtual cpp_ttype
  next (std::string& token) = 0;

public:
  static char const* token_spelling[N_TTYPES + 1];
};


// Adapter to scan a saved token sequence.
//
class cxx_tokens_lexer: public cxx_lexer
{
public:
  void
  start (cxx_tokens const&);

  virtual cpp_ttype
  next (std::string& token);

private:
  cxx_tokens const* tokens_;
  cxx_tokens::const_iterator cur_;
};


// A thin wrapper around the pragma_lex() function that recognizes
// CPP_KEYWORD.
//
class cxx_pragma_lexer: public cxx_lexer
{
public:
  void
  start ();

  // Start with an already extracted (using the pragma_lex() function)
  // token. This function translates the CPP_NAME to CPP_KEYWORD if
  // necessary and returns the string token. It also uses the passed
  // token and type for subsequent calls to next() so after the last
  // next() call they will contain the information about the last
  // token parsed.
  //
  std::string
  start (tree& token, cpp_ttype& type);

  virtual cpp_ttype
  next (std::string& token);

  // This pragma-specific version of next() returns a token as a tree
  // node.
  //
  cpp_ttype
  next (tree& token);

private:
  std::string
  translate ();

private:
  tree* token_;
  cpp_ttype* type_;

  tree token_data_;
  cpp_ttype type_data_;
};

// A thin wrapper around cpp_reader for lexing C++ code fragments.
//
class cxx_string_lexer: public cxx_lexer
{
public:
  cxx_string_lexer ();

  virtual
  ~cxx_string_lexer ();

public:
  void
  start (std::string const&);

  virtual cpp_ttype
  next (std::string& token);

private:
  std::string data_;
  std::string buf_;
  line_maps line_map_;
  cpp_reader* reader_;
  cpp_callbacks* callbacks_;
};

#endif // ODB_CXX_LEXER_HXX
