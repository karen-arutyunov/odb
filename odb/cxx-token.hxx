// file      : odb/cxx-token.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_CXX_TOKEN_HXX
#define ODB_CXX_TOKEN_HXX

#include <string>
#include <vector>

struct cxx_token
{
  unsigned int type;   // Untyped cpp_ttype.
  std::string literal; // Only used for name, string, number, etc.
};

typedef std::vector<cxx_token> cxx_tokens;

#endif // ODB_CXX_TOKEN_HXX
