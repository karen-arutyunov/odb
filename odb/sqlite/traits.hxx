// file      : odb/sqlite/traits.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_TRAITS_HXX
#define ODB_SQLITE_TRAITS_HXX

#include <odb/pre.hxx>

#include <string>
#include <cstddef> // std::size_t

#include <odb/traits.hxx>
#include <odb/details/buffer.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    enum database_type_id
    {
      id_integer,
      id_real,
      id_text,
      id_blob
    };

    //
    // value_traits
    //

    template <typename T, typename I, database_type_id>
    struct default_value_traits;

    template <typename T, typename I, database_type_id ID>
    class value_traits: public default_value_traits<T, I, ID>
    {
    };

    template <typename T, typename I, database_type_id>
    struct default_value_traits
    {
      typedef T value_type;
      typedef T query_type;
      typedef I image_type;

      static void
      set_value (T& v, I i, bool is_null)
      {
        if (!is_null)
          v = T (i);
        else
          v = T ();
      }

      static void
      set_image (I& i, bool& is_null, T v)
      {
        is_null = false;
        i = I (v);
      }
    };

    // std::string specialization.
    //
    template <>
    struct LIBODB_SQLITE_EXPORT default_value_traits<
      std::string, details::buffer, id_text>
    {
      typedef std::string value_type;
      typedef std::string query_type;
      typedef details::buffer image_type;

      static void
      set_value (std::string& v,
                 const details::buffer& b,
                 std::size_t n,
                 bool is_null)
      {
        if (!is_null)
          v.assign (b.data (), n);
        else
          v.erase ();
      }

      static void
      set_image (details::buffer&,
                 std::size_t& n,
                 bool& is_null,
                 const std::string&);
    };

    // const char* specialization
    //
    // Specialization for const char* which only supports initialization
    // of an image from the value but not the other way around. This way
    // we can pass such values to the queries.
    //
    template <>
    struct LIBODB_SQLITE_EXPORT default_value_traits<
      const char*, details::buffer, id_text>
    {
      typedef const char* value_type;
      typedef const char* query_type;
      typedef details::buffer image_type;

      static void
      set_image (details::buffer&,
                 std::size_t& n,
                 bool& is_null,
                 const char*);
    };

    //
    // type_traits
    //

    template <typename T>
    struct default_type_traits;

    template <typename T>
    class type_traits: public default_type_traits<T>
    {
    };

    // Integral types.
    //
    template <>
    struct default_type_traits<bool>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<signed char>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<unsigned char>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<short>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<unsigned short>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<int>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<unsigned int>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<long>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<unsigned long>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<long long>
    {
      static const database_type_id db_type_id = id_integer;
    };

    template <>
    struct default_type_traits<unsigned long long>
    {
      static const database_type_id db_type_id = id_integer;
    };

    // Float types.
    //
    template <>
    struct default_type_traits<float>
    {
      static const database_type_id db_type_id = id_real;
    };

    template <>
    struct default_type_traits<double>
    {
      static const database_type_id db_type_id = id_real;
    };

    // String type.
    //
    template <>
    struct default_type_traits<std::string>
    {
      static const database_type_id db_type_id = id_text;
    };

    template <>
    struct default_type_traits<const char*>
    {
      static const database_type_id db_type_id = id_text;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_TRAITS_HXX