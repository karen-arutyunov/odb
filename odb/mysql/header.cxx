// file      : odb/mysql/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/mysql/common.hxx>
#include <odb/mysql/header.hxx>

namespace mysql
{
  namespace
  {
    const char* integer_types[] =
    {
      "char",
      "short",
      "int",
      "int",
      "long long"
    };

    const char* float_types[] =
    {
      "float",
      "double"
    };

    struct image_member: member_base
    {
      image_member (context& c, bool id)
          : member_base (c, id)
      {
      }

      virtual void
      pre (type& m)
      {
        if (!id_)
          os << "// " << m.name () << endl
             << "//" << endl;
      }

      virtual void
      traverse_integer (type&, sql_type const& t)
      {
        if (t.unsign)
          os << "unsigned ";
        else if (t.type == sql_type::TINYINT)
          os << "signed ";

        os << integer_types[t.type - sql_type::TINYINT] << " " <<
          var << "value;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_float (type&, sql_type const& t)
      {
        os << float_types[t.type - sql_type::FLOAT] << " " << var << "value;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_decimal (type&, sql_type const& t)
      {
        // Exchanged as strings. Can have up to 65 digits not counting
        // '-' and '.'. If range is not specified, the default is 10.
        //
        os << "char " << var << "value[" <<
          (t.range ? t.range_value : 10) + 3 << "];"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_date_time (type&, sql_type const& t)
      {
        if (t.type == sql_type::YEAR)
          os << "short ";
        else
          os << "MYSQL_TIME ";

        os << var << "value;"
           << "my_bool " << var << "null;"
           << endl;

      }

      virtual void
      traverse_short_string (type&, sql_type const& t)
      {
        // If range is not specified, the default buffer size is 255.
        //
        os << "char " << var << "value[" <<
          (t.range ? t.range_value : 255) + 1 << "];"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_long_string (type&, sql_type const& t)
      {
        os << "odb::buffer " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_bit (type&, sql_type const& t)
      {
        // Valid range is 1 to 64.
        //
        unsigned int n (t.range / 8 + (t.range % 8 ? 1 : 0));

        os << "unsigned char " << var << "value[" << n << "];"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_enum (type&, sql_type const&)
      {
        // Represented as string.
        //
        os << "odb::buffer " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_set (type&, sql_type const&)
      {
        // Represented as string.
        //
        os << "odb::buffer " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }
    };

    struct image_type: traversal::class_, context
    {
      image_type (context& c)
          : context (c), image_member_ (c, false)
      {
        *this >> names_image_member_ >> image_member_;
      }

      virtual void
      traverse (type& c)
      {
        os << "struct image_type"
           << "{";

        names (c);

        os << "};";
      }

    private:
      image_member image_member_;
      traversal::names names_image_member_;
    };

    struct id_image_type: traversal::class_, context
    {
      id_image_type (context& c)
          : context (c), image_member_ (c, true)
      {
        *this >> names_image_member_ >> image_member_;
      }

      virtual void
      traverse (type& c)
      {
        os << "struct id_image_type"
           << "{";

        names (c);

        os << "};";
      }

    private:
      image_member image_member_;
      traversal::names names_image_member_;
    };

    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c),
            id_member_ (c),
            member_count_ (c),
            image_type_ (c),
            id_image_type_ (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!c.count ("object"))
          return;

        string const& type (c.fq_name ());

        // Find the id member and type.
        //
        id_member_.traverse (c);

        if (id_member_.member () == 0)
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << " error: no data member designated as object id" << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << " info: use '#pragma odb id' to specify object id member"
               << endl;
        }

        semantics::data_member& id (*id_member_.member ());
        semantics::type& id_type (id.type ());

        if (id_type.anonymous ())
        {
          // Can be a template-id (which we should handle eventually) or an
          // anonymous type in member declaration (e.g., struct {...} m_;).
          //
          cerr << id.file () << ":" << id.line () << ":" << id.column ()
               << " error: unnamed type in data member declaration" << endl;

          cerr << id.file () << ":" << id.line () << ":" << id.column ()
               << " info: use 'typedef' to name this type"
               << endl;

          throw generation_failed ();
        }

        member_count_.traverse (c);
        size_t column_count (member_count_.count ());

        if (column_count == 0)
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << " error: no persistent data members in the class" << endl;

          throw generation_failed ();
        }

        bool has_grow;
        {
          has_grow_member m (*this);
          traversal::names n (m);
          names (c, n);
          has_grow = m.result ();
        }

        os << "// " << c.name () << endl
           << "//" << endl;

        os << "template <>" << endl
           << "class access::object_traits< " << type << " >: " << endl
           << "  public access::object_memory< " << type << " >," << endl
           << "  public access::object_factory< " << type << " >"
           << "{"
           << "public:" << endl;

        // object_type & shared_ptr
        //
        os << "typedef " << type << " object_type;";

        // id_type
        //
        os << "typedef " << id_type.fq_name () << " id_type;"
           << endl;

        // image_type
        //
        image_type_.traverse (c);

        // id_image_type
        //
        id_image_type_.traverse (c);

        // id_source
        //
        os << "static const odb::id_source id_source = odb::ids_assigned;"
           << endl;

        // column_count
        //
        os << "static const std::size_t column_count = " << column_count <<
          "UL;"
           << endl;

        // Queries.
        //
        os << "static const char* const insert_query;"
           << "static const char* const select_query;"
           << "static const char* const update_query;"
           << "static const char* const delete_query;"
           << endl;

        // id ()
        //
        os << "static id_type" << endl
           << "id (const object_type&);"
           << endl;

        // grow ()
        //
        if (has_grow)
        {
          os << "static bool" << endl
             << "grow (image_type&, my_bool*);"
             << endl;
        }

        // bind (image_type)
        //
        os << "static void" << endl
           << "bind (mysql::binding&, image_type&);"
           << endl;

        // bind (id_image_type)
        //
        os << "static void" << endl
           << "bind (mysql::binding&, id_image_type&);"
           << endl;

        // init (image, object)
        //
        os << "static bool" << endl
           << "init (image_type&, object_type&);"
           << endl;

        // init (object, image)
        //
        os << "static void" << endl
           << "init (object_type&, image_type&);"
           << endl;

        // persist ()
        //
        os << "static void" << endl
           << "persist (database&, object_type&);"
           << endl;

        // store ()
        //
        os << "static void" << endl
           << "store (database&, object_type&);"
           << endl;

        // erase ()
        //
        os << "static void" << endl
           << "erase (database&, const id_type&);"
           << endl;

        // find ()
        //
        os << "static pointer_type" << endl
           << "find (database&, const id_type&);"
           << endl;

        os << "static bool" << endl
           << "find (database&, const id_type&, object_type&);"
           << endl;

        os << "private:" << endl
           << "static bool" << endl
           << "find (mysql::object_statements<object_type>&, const id_type&);";

        os << "};";
      }

    private:
      id_member id_member_;
      member_count member_count_;
      image_type image_type_;
      id_image_type id_image_type_;
    };
  }

  void
  generate_header (context& ctx)
  {
    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::namespace_ ns;
    class_ c (ctx);

    unit >> unit_defines >> ns;
    unit_defines >> c;

    traversal::defines ns_defines;

    ns >> ns_defines >> ns;
    ns_defines >> c;

    ctx.os << "#include <cstddef>" << endl // std::size_t
           << endl;

    ctx.os << "#include <mysql/mysql.h>" << endl
           << endl;

    ctx.os << "#include <odb/core.hxx>" << endl
           << "#include <odb/traits.hxx>" << endl
           << "#include <odb/buffer.hxx>" << endl
           << endl
           << "#include <odb/mysql/version.hxx>" << endl
           << "#include <odb/mysql/forward.hxx>" << endl
           << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
