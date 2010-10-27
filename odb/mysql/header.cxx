// file      : odb/mysql/header.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <odb/mysql/common.hxx>
#include <odb/mysql/header.hxx>

namespace mysql
{
  namespace
  {
    struct image_member: member_base
    {
      image_member (context& c, bool id)
          : member_base (c, id), member_image_type_ (c, id)
      {
      }

      virtual void
      pre (type& m)
      {
        image_type = member_image_type_.image_type (m);

        if (!id_)
          os << "// " << m.name () << endl
             << "//" << endl;
      }

      virtual void
      traverse_composite (type&)
      {
        os << image_type << " " << var << "value;"
           << endl;
      }

      virtual void
      traverse_integer (type&, sql_type const&)
      {
        os << image_type << " " << var << "value;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_float (type&, sql_type const&)
      {
        os << image_type << " " << var << "value;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_decimal (type&, sql_type const&)
      {
        // Exchanged as strings. Can have up to 65 digits not counting
        // '-' and '.'. If range is not specified, the default is 10.
        //

        /*
          @@ Disabled.
        os << "char " << var << "value[" <<
          (t.range ? t.range_value : 10) + 3 << "];"
        */

        os << image_type << " " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_date_time (type&, sql_type const&)
      {
        os << image_type << " " << var << "value;"
           << "my_bool " << var << "null;"
           << endl;

      }

      virtual void
      traverse_short_string (type&, sql_type const&)
      {
        // If range is not specified, the default buffer size is 255.
        //
        /*
          @@ Disabled.
        os << "char " << var << "value[" <<
          (t.range ? t.range_value : 255) + 1 << "];"
        */

        os << image_type << " " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_long_string (type&, sql_type const&)
      {
        os << image_type << " " << var << "value;"
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
        os << image_type << " " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

      virtual void
      traverse_set (type&, sql_type const&)
      {
        // Represented as string.
        //
        os << image_type << " " << var << "value;"
           << "unsigned long " << var << "size;"
           << "my_bool " << var << "null;"
           << endl;
      }

    private:
      string image_type;

      member_image_type member_image_type_;
    };

    struct image_base: traversal::class_, context
    {
      image_base (context& c): context (c), first_ (true) {}

      virtual void
      traverse (type& c)
      {
        if (first_)
        {
          os << ": ";
          first_ = false;
        }
        else
        {
          os << "," << endl
             << "  ";
        }

        os << "composite_value_traits< " << c.fq_name () << " >::image_type";
      }

    private:
      bool first_;
    };

    struct image_type: traversal::class_, context
    {
      image_type (context& c)
          : context (c), member_ (c, false)
      {
        *this >> names_member_ >> member_;
      }

      virtual void
      traverse (type& c)
      {
        os << "struct image_type";

        {
          image_base b (*this);
          traversal::inherits i (b);
          inherits (c, i);
        }

        os << "{";

        names (c);

        os << "};";
      }

    private:
      image_member member_;
      traversal::names names_member_;
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
            image_type_ (c),
            id_image_type_ (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (c.count ("object"))
          traverse_object (c);
        else if (comp_value (c))
          traverse_value (c);
      }

      virtual void
      traverse_object (type& c)
      {
        string const& type (c.fq_name ());
        bool def_ctor (TYPE_HAS_DEFAULT_CONSTRUCTOR (c.tree_node ()));

        id_member_.traverse (c);
        semantics::data_member& id (*id_member_.member ());
        bool auto_id (id.count ("auto"));

        os << "// " << c.name () << endl
           << "//" << endl;

        os << "template <>" << endl
           << "class access::object_traits< " << type << " >: " << endl
           << "  public access::object_memory< " << type << " >," << endl
           << "  public access::object_factory< " << type << " >"
           << "{"
           << "public:" << endl;

        // object_type
        //
        os << "typedef " << type << " object_type;";

        // id_type
        //
        os << "typedef " << id.type ().fq_name (id.belongs ().hint ()) <<
          " id_type;"
           << endl;

        // image_type
        //
        image_type_.traverse (c);

        // id_image_type
        //
        id_image_type_.traverse (c);

        // query_type & query_base_type
        //
        if (options.generate_query ())
        {
          // query_base_type
          //
          os << "typedef mysql::query query_base_type;"
             << endl;

          // query_type
          //
          os << "struct query_type: query_base_type"
             << "{";

          {
            query_columns t (*this);
            t.traverse (c);
          }

          os << "query_type ();"
             << "query_type (const std::string&);"
             << "query_type (const query_base_type&);"
             << "};";
        }

        // column_count
        //
        os << "static const std::size_t column_count = " <<
          column_count (c) << "UL;"
           << endl;

        // Queries.
        //
        os << "static const char* const persist_statement;"
           << "static const char* const find_statement;"
           << "static const char* const update_statement;"
           << "static const char* const erase_statement;";

        if (options.generate_query ())
          os << "static const char* const query_clause;";

        os << endl;

        // id ()
        //
        os << "static id_type" << endl
           << "id (const object_type&);"
           << endl;

        // grow ()
        //
        os << "static bool" << endl
           << "grow (image_type&, my_bool*);"
           << endl;

        // bind (image_type)
        //
        os << "static void" << endl
           << "bind (MYSQL_BIND*, image_type&);"
           << endl;

        // bind (id_image_type)
        //
        os << "static void" << endl
           << "bind (MYSQL_BIND*, id_image_type&);"
           << endl;

        // init (image, object)
        //
        os << "static bool" << endl
           << "init (image_type&, const object_type&);"
           << endl;

        // init (object, image)
        //
        os << "static void" << endl
           << "init (object_type&, const image_type&);"
           << endl;

        // persist ()
        //
        os << "static void" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type&);"
           << endl;

        // update ()
        //
        os << "static void" << endl
           << "update (database&, const object_type&);"
           << endl;

        // erase ()
        //
        os << "static void" << endl
           << "erase (database&, const id_type&);"
           << endl;

        // find ()
        //
        if (def_ctor)
          os << "static pointer_type" << endl
             << "find (database&, const id_type&);"
             << endl;

        os << "static bool" << endl
           << "find (database&, const id_type&, object_type&);"
           << endl;

        // query ()
        //
        if (options.generate_query ())
          os << "static result<object_type>" << endl
             << "query (database&, const query_type&);"
             << endl;

        // Helpers.
        //
        os << "private:" << endl
           << "static bool" << endl
           << "find (mysql::object_statements<object_type>&, const id_type&);";

        os << "};";
      }

      virtual void
      traverse_value (type& c)
      {
        string const& type (c.fq_name ());

        os << "// " << c.name () << endl
           << "//" << endl;

        os << "template <>" << endl
           << "class access::composite_value_traits< " << type << " >"
           << "{"
           << "public:" << endl;

        // object_type
        //
        os << "typedef " << type << " value_type;"
           << endl;

        // image_type
        //
        image_type_.traverse (c);

        // grow ()
        //
        os << "static bool" << endl
           << "grow (image_type&, my_bool*);"
           << endl;

        // bind (image_type)
        //
        os << "static void" << endl
           << "bind (MYSQL_BIND*, image_type&);"
           << endl;

        // init (image, object)
        //
        os << "static bool" << endl
           << "init (image_type&, const value_type&);"
           << endl;

        // init (object, image)
        //
        os << "static void" << endl
           << "init (value_type&, const image_type&);"
           << endl;

        os << "};";
      }

    private:
      id_member id_member_;
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

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/result.hxx>" << endl
             << endl;

    ctx.os << "#include <odb/mysql/version.hxx>" << endl
           << "#include <odb/mysql/forward.hxx>" << endl
           << "#include <odb/mysql/mysql-types.hxx>" << endl;

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/mysql/query.hxx>" << endl;

    ctx.os << endl
           << "#include <odb/details/buffer.hxx>" << endl
           << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
