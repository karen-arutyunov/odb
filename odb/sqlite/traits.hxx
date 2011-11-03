// file      : odb/sqlite/traits.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_TRAITS_HXX
#define ODB_SQLITE_TRAITS_HXX

#include <odb/pre.hxx>

#include <string>
#include <vector>
#include <cstddef> // std::size_t

#include <odb/traits.hxx>
#include <odb/wrapper-traits.hxx>

#include <odb/details/buffer.hxx>
#include <odb/details/wrapper-p.hxx>

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
    // image_traits
    //

    template <database_type_id>
    struct image_traits;

    template <>
    struct image_traits<id_integer> {typedef long long image_type;};

    template <>
    struct image_traits<id_real> {typedef double image_type;};

    template <>
    struct image_traits<id_text> {typedef details::buffer image_type;};

    template <>
    struct image_traits<id_blob> {typedef details::buffer image_type;};

    //
    // value_traits
    //

    template <typename W, database_type_id, bool null_handler>
    struct wrapped_value_traits;

    template <typename T, database_type_id>
    struct default_value_traits;

    template <typename T, database_type_id, bool w = details::wrapper_p<T>::r>
    struct select_traits;

    template <typename T, database_type_id ID>
    struct select_traits<T, ID, false>
    {
      typedef default_value_traits<T, ID> type;
    };

    template <typename W, database_type_id ID>
    struct select_traits<W, ID, true>
    {
      typedef
      wrapped_value_traits<W, ID, wrapper_traits<W>::null_handler>
      type;
    };

    template <typename T, database_type_id ID>
    class value_traits: public select_traits<T, ID>::type
    {
    };

    // The wrapped_value_traits specializations should be able to handle
    // any value type which means we have to have every possible signature
    // of the set_value() and set_image() functions.
    //
    template <typename W, database_type_id ID>
    struct wrapped_value_traits<W, ID, false>
    {
      typedef wrapper_traits<W> wtraits;
      typedef typename wtraits::unrestricted_wrapped_type wrapped_type;

      typedef W value_type;
      typedef wrapped_type query_type;
      typedef typename image_traits<ID>::image_type image_type;

      typedef value_traits<wrapped_type, ID> vtraits;

      static void
      set_value (W& v, const image_type& i, bool is_null)
      {
        vtraits::set_value (wtraits::set_ref (v), i, is_null);
      }

      static void
      set_image (image_type& i, bool& is_null, const W& v)
      {
        vtraits::set_image (i, is_null, wtraits::get_ref (v));
      }

      // TEXT and BLOB.
      //
      static void
      set_value (W& v, const details::buffer& b, std::size_t n, bool is_null)
      {
        vtraits::set_value (wtraits::set_ref (v), b, n, is_null);
      }

      static void
      set_image (details::buffer& b, std::size_t& n, bool& is_null, const W& v)
      {
        vtraits::set_image (b, n, is_null, wtraits::get_ref (v));
      }
    };

    template <typename W, database_type_id ID>
    struct wrapped_value_traits<W, ID, true>
    {
      typedef wrapper_traits<W> wtraits;
      typedef typename wtraits::unrestricted_wrapped_type wrapped_type;

      typedef W value_type;
      typedef wrapped_type query_type;
      typedef typename image_traits<ID>::image_type image_type;

      typedef value_traits<wrapped_type, ID> vtraits;

      static void
      set_value (W& v, const image_type& i, bool is_null)
      {
        if (is_null)
          wtraits::set_null (v);
        else
          vtraits::set_value (wtraits::set_ref (v), i, is_null);
      }

      static void
      set_image (image_type& i, bool& is_null, const W& v)
      {
        is_null = wtraits::get_null (v);

        if (!is_null)
          vtraits::set_image (i, is_null, wtraits::get_ref (v));
      }

      // TEXT and BLOB.
      //
      static void
      set_value (W& v, const details::buffer& b, std::size_t n, bool is_null)
      {
        if (is_null)
          wtraits::set_null (v);
        else
          vtraits::set_value (wtraits::set_ref (v), b, n, is_null);
      }

      static void
      set_image (details::buffer& b, std::size_t& n, bool& is_null, const W& v)
      {
        is_null = wtraits::get_null (v);

        if (!is_null)
          vtraits::set_image (b, n, is_null, wtraits::get_ref (v));
      }
    };

    template <typename T, database_type_id ID>
    struct default_value_traits
    {
      typedef T value_type;
      typedef T query_type;
      typedef typename image_traits<ID>::image_type image_type;

      static void
      set_value (T& v, const image_type& i, bool is_null)
      {
        if (!is_null)
          v = T (i);
        else
          v = T ();
      }

      static void
      set_image (image_type& i, bool& is_null, T v)
      {
        is_null = false;
        i = image_type (v);
      }
    };

    // std::string specialization.
    //
    template <>
    struct LIBODB_SQLITE_EXPORT default_value_traits<std::string, id_text>
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
    struct LIBODB_SQLITE_EXPORT c_string_value_traits
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

    template <>
    struct LIBODB_SQLITE_EXPORT default_value_traits<const char*, id_text>:
      c_string_value_traits
    {
    };

    template <std::size_t n>
    struct default_value_traits<char[n], id_text>: c_string_value_traits
    {
    };

    template <std::size_t n>
    struct default_value_traits<const char[n], id_text>: c_string_value_traits
    {
    };

    // std::vector<char> (buffer) specialization.
    //
    template <>
    struct LIBODB_SQLITE_EXPORT default_value_traits<
      std::vector<char>, id_blob>
    {
    public:
      typedef std::vector<char> value_type;
      typedef std::vector<char> query_type;
      typedef details::buffer image_type;

      static void
      set_value (value_type& v,
                 const details::buffer& b,
                 std::size_t n,
                 bool is_null)
      {
        if (!is_null)
          v.assign (b.data (), b.data () + n);
        else
          v.clear ();
      }

      static void
      set_image (details::buffer&,
                 std::size_t& n,
                 bool& is_null,
                 const value_type&);
    };

    // std::vector<unsigned char> (buffer) specialization.
    //
    template <>
    struct LIBODB_SQLITE_EXPORT default_value_traits<
      std::vector<unsigned char>, id_blob>
    {
    public:
      typedef std::vector<unsigned char> value_type;
      typedef std::vector<unsigned char> query_type;
      typedef details::buffer image_type;

      static void
      set_value (value_type& v,
                 const details::buffer& b,
                 std::size_t n,
                 bool is_null)
      {
        if (!is_null)
        {
          const unsigned char* d (
            reinterpret_cast<const unsigned char*> (b.data ()));
          v.assign (d, d + n);
        }
        else
          v.clear ();
      }

      static void
      set_image (details::buffer&,
                 std::size_t& n,
                 bool& is_null,
                 const value_type&);
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

    template <std::size_t n>
    struct default_type_traits<char[n]>
    {
      static const database_type_id db_type_id = id_text;
    };

    template <std::size_t n>
    struct default_type_traits<const char[n]>
    {
      static const database_type_id db_type_id = id_text;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_TRAITS_HXX
