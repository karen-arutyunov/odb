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

    struct image_member_base: traversal::data_member, context
    {
      image_member_base (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& m)
      {
        string const& name (m.name ());
        var = name + (name[name.size () - 1] == '_' ? "" : "_");

        os << "// " << name << endl
           << "//" << endl;

        sql_type const& t (db_type (m));

        switch (t.type)
        {
          // Integral types.
          //
        case sql_type::TINYINT:
        case sql_type::SMALLINT:
        case sql_type::MEDIUMINT:
        case sql_type::INT:
        case sql_type::BIGINT:
          {
            traverse_integer (m, t);
            break;
          }

          // Float types.
          //
        case sql_type::FLOAT:
        case sql_type::DOUBLE:
          {
            traverse_float (m, t);
            break;
          }
        case sql_type::DECIMAL:
          {
            traverse_decimal (m, t);
            break;
          }

          // Data-time types.
          //
        case sql_type::DATE:
        case sql_type::TIME:
        case sql_type::DATETIME:
        case sql_type::TIMESTAMP:
        case sql_type::YEAR:
          {
            traverse_date_time (m, t);
            break;
          }

          // String and binary types.
          //
        case sql_type::CHAR:
        case sql_type::VARCHAR:
        case sql_type::TINYTEXT:
        case sql_type::TEXT:
        case sql_type::MEDIUMTEXT:
        case sql_type::LONGTEXT:
          {
            // For string types the limit is in characters rather
            // than in bytes. The fixed-length pre-allocated buffer
            // optimization can only be used for 1-byte encodings.
            // To support this we will need the character encoding
            // in sql_type.
            //
            traverse_long_string (m, t);
            break;
          }
        case sql_type::BINARY:
          {
            // BINARY's range is always 255 or less from MySQL 5.0.3.
            //
            traverse_short_string (m, t);
            break;
          }
        case sql_type::VARBINARY:
        case sql_type::TINYBLOB:
        case sql_type::BLOB:
        case sql_type::MEDIUMBLOB:
        case sql_type::LONGBLOB:
          {
            if (t.range && t.range_value <= 255)
              traverse_short_string (m, t);
            else
              traverse_long_string (m, t);

            break;
          }

          // Other types.
          //
        case sql_type::BIT:
          {
            traverse_bit (m, t);
            break;
          }
        case sql_type::ENUM:
          {
            traverse_enum (m, t);
            break;
          }
        case sql_type::SET:
          {
            traverse_set (m, t);
            break;
          }
        }
      }

      virtual void
      traverse_integer (type&, sql_type const&)
      {
      }

      virtual void
      traverse_float (type&, sql_type const&)
      {
      }

      virtual void
      traverse_decimal (type&, sql_type const&)
      {
      }

      virtual void
      traverse_date_time (type&, sql_type const&)
      {
      }

      virtual void
      traverse_short_string (type&, sql_type const&)
      {
      }

      virtual void
      traverse_long_string (type&, sql_type const&)
      {
      }

      virtual void
      traverse_bit (type&, sql_type const&)
      {
      }

      virtual void
      traverse_enum (type&, sql_type const&)
      {
      }

      virtual void
      traverse_set (type&, sql_type const&)
      {
      }

    protected:
      string var;
    };

    struct image_member: image_member_base
    {
      image_member (context& c)
          : image_member_base (c)
      {
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
        os << "const char* " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << "char " << var << "buffer[" <<
          (t.range ? t.range_value : 10) + 3 << "];"
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
        os << "const char* " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << "char " << var << "buffer[" <<
          (t.range ? t.range_value : 255) + 1 << "];"
           << endl;
      }

      virtual void
      traverse_long_string (type&, sql_type const& t)
      {
        os << "const char* " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << "odb::buffer " << var << "buffer;"
           << endl;
      }

      virtual void
      traverse_bit (type&, sql_type const& t)
      {
        // Valid range is 1 to 64.
        //
        unsigned int n (t.range / 8 + (t.range % 8 ? 1 : 0));

        os << "unsigned char " << var << "value[" << n << "];"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_enum (type&, sql_type const&)
      {
        // Represented as string.
        //
        os << "const char* " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << "odb::buffer " << var << "buffer;"
           << endl;
      }

      virtual void
      traverse_set (type&, sql_type const&)
      {
        // Represented as string.
        //
        os << "const char* " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << "odb::buffer " << var << "buffer;"
           << endl;
      }
    };

    struct image_type: traversal::class_, context
    {
      image_type (context& c)
          : context (c), image_member_ (c)
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

    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c), image_type_ (c)
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
        id_member t (*this);
        t.traverse (c);

        if (t.member () == 0)
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << " error: no data member designated as object id" << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << " info: use '#pragma odb id' to specify object id member"
               << endl;

          throw generation_failed ();
        }

        semantics::data_member& id (*t.member ());
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

        // id_source
        //
        os << "static const odb::id_source id_source = odb::ids_assigned;"
           << endl;

        // id ()
        //
        os << "static id_type" << endl
           << "id (const object_type&);"
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
           << "find (database&, const id_type&, object_type&);";

        os << "};";
      }

    private:
      image_type image_type_;
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

    ctx.os << "#include <mysql/mysql.h>" << endl
           << endl;

    ctx.os << "#include <odb/core.hxx>" << endl
           << "#include <odb/traits.hxx>" << endl
           << "#include <odb/buffer.hxx>" << endl
           << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
