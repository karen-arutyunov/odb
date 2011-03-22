// file      : odb/sqlite/binding.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_BINDING_HXX
#define ODB_SQLITE_BINDING_HXX

#include <odb/pre.hxx>

#include <cstddef>  // std::size_t

#include <odb/forward.hxx>

#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    // The SQLite parameter/result binding. This data structures is
    // modelled after MYSQL_BIND from MySQL.
    //
    struct bind
    {
      enum buffer_type
      {
        integer, // Buffer is long long; size, capacity, truncated are unused.
        real,    // Buffer is double; size, capacity, truncated are unused.
        text,    // Buffer is a char array.
        blob     // Buffer is a char array.
      };

      buffer_type type;
      void* buffer;
      std::size_t* size;
      std::size_t capacity;
      bool* is_null;
      bool* truncated;
    };

    class LIBODB_SQLITE_EXPORT binding
    {
    public:
      typedef sqlite::bind bind_type;

      binding (bind_type* b, std::size_t n)
          : bind (b), count (n), version (0)
      {
      }

      bind_type* bind;
      std::size_t count;
      std::size_t version;

    private:
      binding (const binding&);
      binding& operator= (const binding&);
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_BINDING_HXX
