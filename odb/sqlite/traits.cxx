// file      : odb/sqlite/traits.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cstring> // std::memcpy, std::strlen

#include <odb/sqlite/traits.hxx>

using namespace std;

namespace odb
{
  namespace sqlite
  {
    using details::buffer;

    // default_value_traits<std::string>
    //
    void default_value_traits<string, buffer, id_text>::
    set_image (buffer& b,
               size_t& n,
               bool& is_null,
               const string& v)
    {
      is_null = false;
      n = v.size ();

      if (n > b.capacity ())
        b.capacity (n);

      if (n != 0)
        memcpy (b.data (), v.c_str (), n);
    }

    // default_value_traits<const char*>
    //
    void default_value_traits<const char*, buffer, id_text>::
    set_image (buffer& b,
               size_t& n,
               bool& is_null,
               const char* v)
    {
      is_null = false;
      n = strlen (v);

      if (n > b.capacity ())
        b.capacity (n);

      if (n != 0)
        memcpy (b.data (), v, n);
    }
  }
}
