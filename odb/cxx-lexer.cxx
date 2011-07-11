// file      : odb/cxx-lexer.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <stdio.h>
#include <stdarg.h>

#include <new>      // std::bad_alloc
#include <cassert>
#include <iostream>

#include <odb/cxx-lexer.hxx>

using namespace std;

// Token spelling. See cpplib.h for details.
//
#define OP(e, s) s ,
#define TK(e, s) #e ,
char const* cxx_lexer::token_spelling[N_TTYPES + 1] = { TTYPE_TABLE "KEYWORD"};
#undef OP
#undef TK

// Diagnostics callback.
//
extern "C" bool
cpp_error_callback (
  cpp_reader* reader,
  int level,
#if BUILDING_GCC_MAJOR > 4 || BUILDING_GCC_MAJOR == 4 && BUILDING_GCC_MINOR > 5
  int /*reason*/, // Added in GCC 4.6.0.
#endif
  location_t,
  unsigned int,
  char const* msg,
  va_list *ap)
{
  char const* kind (0);
  switch (level)
  {
  case CPP_DL_NOTE:
  case CPP_DL_WARNING_SYSHDR:
  case CPP_DL_WARNING:
  case CPP_DL_PEDWARN:
    // Ignore these.
    break;
  case CPP_DL_ERROR:
  case CPP_DL_FATAL:
    kind = "error";
    break;
  case CPP_DL_ICE:
    kind = "ice";
    break;
  default:
    kind = "unknown";
    break;
  }

  if (kind != 0)
  {
    fprintf (stderr, "%s: ", kind);
    vfprintf (stderr, msg, *ap);
    fprintf (stderr, "\n");

    // By resetting the error callback we indicate to cxx_lexer
    // that there was an error.
    //
    cpp_get_callbacks (reader)->error = 0;
    return true;
  }

  return false;
}

cxx_lexer::
cxx_lexer ()
    : reader_ (0)
{
  linemap_init (&line_map_);
  linemap_add (&line_map_, LC_ENTER, 0, "<memory>", 0);

  reader_ = cpp_create_reader (
    cxx_dialect == cxx0x ? CLK_CXX0X : CLK_CXX98, 0, &line_map_);

  if (reader_ == 0)
    throw bad_alloc ();

  callbacks_ = cpp_get_callbacks (reader_);
}

cxx_lexer::
~cxx_lexer ()
{
  if (reader_ != 0)
    cpp_destroy (reader_);

  linemap_free (&line_map_);
}

void cxx_lexer::
start (string const& data)
{
  // The previous lexing session should have popped the buffer.
  //
  assert (cpp_get_buffer (reader_) == 0);
  callbacks_->error = &cpp_error_callback;

  data_ = data;
  buf_ = data;
  buf_ += '\n';

  cpp_push_buffer (
    reader_,
    reinterpret_cast<unsigned char const*> (buf_.c_str ()),
    buf_.size (),
    true);
}

cpp_ttype cxx_lexer::
next (string& token)
{
  token.clear ();
  cpp_token const* t (cpp_get_token (reader_));

  // If there was an error, the error callback will be reset to 0.
  // Diagnostics has already been issued.
  //
  if (callbacks_->error == 0)
    throw invalid_input ();

  cpp_ttype tt (t->type);

  // @@ Need to handle literals, at least integer.
  //
  switch (tt)
  {
  case CPP_NAME:
    {
      char const* name (
        reinterpret_cast<char const*> (NODE_NAME (t->val.node.node)));

      // See if this is a keyword using the C++ parser machinery and
      // the current C++ dialect.
      //
      tree id (get_identifier (name));

      if (C_IS_RESERVED_WORD (id))
        tt = CPP_KEYWORD;

      token = name;
      break;
    }
  case CPP_NUMBER:
    {
      cpp_string const& s (t->val.str);
      token.assign (reinterpret_cast<char const*> (s.text), s.len);
      break;
    }
  default:
    {
      if (tt <= CPP_LAST_PUNCTUATOR)
        token += token_spelling[tt];
      else
      {
        cerr << "unexpected token '" << token_spelling[tt] << "' in '" <<
          data_ << "'" << endl;
        throw invalid_input ();
      }
      break;
    }
  }

  return tt;
}
