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
      image_member (context& c, string const& var = string ())
          : member_base (c, var), member_image_type_ (c)
      {
      }

      image_member (context& c,
                    string const& var,
                    semantics::type& t,
                    string const& fq_type,
                    string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix),
            member_image_type_ (c, t, fq_type, key_prefix)
      {
      }

      virtual void
      pre (member_info& mi)
      {
        if (container (mi.t))
          return;

        image_type = member_image_type_.image_type (mi.m);

        if (var_override_.empty ())
          os << "// " << mi.m.name () << endl
             << "//" << endl;
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << image_type << " " << mi.var << "value;"
           << endl;
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        os << image_type << " " << mi.var << "value;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << image_type << " " << mi.var << "value;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        // Exchanged as strings. Can have up to 65 digits not counting
        // '-' and '.'. If range is not specified, the default is 10.
        //

        /*
          @@ Disabled.
        os << "char " << mi.var << "value[" <<
          (t.range ? t.range_value : 10) + 3 << "];"
        */

        os << image_type << " " << mi.var << "value;"
           << "unsigned long " << mi.var << "size;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << image_type << " " << mi.var << "value;"
           << "my_bool " << mi.var << "null;"
           << endl;

      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // If range is not specified, the default buffer size is 255.
        //
        /*
          @@ Disabled.
        os << "char " << mi.var << "value[" <<
          (t.range ? t.range_value : 255) + 1 << "];"
        */

        os << image_type << " " << mi.var << "value;"
           << "unsigned long " << mi.var << "size;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << image_type << " " << mi.var << "value;"
           << "unsigned long " << mi.var << "size;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Valid range is 1 to 64.
        //
        unsigned int n (mi.st->range / 8 + (mi.st->range % 8 ? 1 : 0));

        os << "unsigned char " << mi.var << "value[" << n << "];"
           << "unsigned long " << mi.var << "size;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as string.
        //
        os << image_type << " " << mi.var << "value;"
           << "unsigned long " << mi.var << "size;"
           << "my_bool " << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as string.
        //
        os << image_type << " " << mi.var << "value;"
           << "unsigned long " << mi.var << "size;"
           << "my_bool " << mi.var << "null;"
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
          : context (c), member_ (c)
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

    // Member-specific traits types for container members.
    //
    struct container_traits: object_members_base, context
    {
      container_traits (context& c)
          : object_members_base (c, true, false), context (c)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        using semantics::type;
        using semantics::class_;

        type& t (m.type ());
        container_kind_type ck (container_kind (t));

        type& vt (container_vt (t));
        type* it (0);
        type* kt (0);

        switch (ck)
        {
        case ck_ordered:
          {
            it = &container_it (t);
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            kt = &container_kt (t);
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        string name (prefix_ + public_name (m) + "_traits");

        // Figure out column counts.
        //
        size_t data_columns (1), cond_columns (1); // One for object id.

        switch (ck)
        {
        case ck_ordered:
          {
            // Add one for the index.
            //
            data_columns++;
            cond_columns++;
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            // Add some for the key.
            //
            size_t n;

            if (class_* kc = comp_value (*kt))
              n = column_count (*kc);
            else
              n = 1;

            data_columns += n;
            cond_columns += n;
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            // Value is also a key.
            //
            if (class_* vc = comp_value (vt))
              cond_columns += column_count (*vc);
            else
              cond_columns++;

            break;
          }
        }

        if (class_* vc = comp_value (vt))
          data_columns += column_count (*vc);
        else
          data_columns++;

        // Store column counts for the source generator.
        //
        m.set ("cond-column-count", cond_columns);
        m.set ("data-column-count", data_columns);

        os << "// " << m.name () << endl
           << "//" << endl
           << "struct " << name
           << "{";

        // container_type
        // container_traits
        // index_type
        // key_type
        // value_type
        //

        os << "typedef " << t.fq_name (m.belongs ().hint ()) <<
          " container_type;";
        os << "typedef odb::access::container_traits< container_type > " <<
          "container_traits;";

        switch (ck)
        {
        case ck_ordered:
          {
            os << "typedef container_traits::index_type index_type;";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "typedef container_traits::key_type key_type;";
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        os << "typedef container_traits::value_type value_type;"
           << endl;

        // functions_type
        //
        switch (ck)
        {
        case ck_ordered:
          {
            os << "typedef ordered_functions<index_type, value_type> " <<
              "functions_type;";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "typedef map_functions<key_type, value_type> " <<
              "functions_type;";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "typedef set_functions<value_type> functions_type;";
            break;
          }
        }

        os << "typedef mysql::container_statements< " << name <<
          " > statements_type;"
           << endl;

        // column_count
        //
        os << "static const std::size_t cond_column_count = " <<
          cond_columns << "UL;"
           << "static const std::size_t data_column_count = " <<
          data_columns << "UL;"
           << endl;

        // cond_image_type (object id is taken from the object image)
        //
        os << "struct cond_image_type"
           << "{";

        switch (ck)
        {
        case ck_ordered:
          {
            os << "// index" << endl
               << "//" << endl;
            image_member im (*this, "index_", *it, "index_type", "index");
            im.traverse (m);
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "// key" << endl
               << "//" << endl;
            image_member im (*this, "key_", *kt, "key_type", "key");
            im.traverse (m);
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "// value" << endl
               << "//" << endl;
            image_member im (*this, "value_", vt, "value_type", "value");
            im.traverse (m);
            break;
          }
        }

        os << "};";

        // data_image_type (object id is taken from the object image)
        //
        os << "struct data_image_type"
           << "{";

        switch (ck)
        {
        case ck_ordered:
          {
            os << "// index" << endl
               << "//" << endl;
            image_member im (*this, "index_", *it, "index_type", "index");
            im.traverse (m);
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "// key" << endl
               << "//" << endl;
            image_member im (*this, "key_", *kt, "key_type", "key");
            im.traverse (m);
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        os << "// value" << endl
           << "//" << endl;
        image_member im (*this, "value_", vt, "value_type", "value");
        im.traverse (m);

        os << "};";

        // Statements.
        //
        os << "static const char* const insert_one_statement;"
           << "static const char* const select_all_statement;"
           << "static const char* const delete_all_statement;"
           << endl;

        // bind (cond_image)
        //
        os << "static void" << endl
           << "bind (MYSQL_BIND*, id_image_type*, cond_image_type&);"
           << endl;

        // bind (data_image)
        //
        os << "static void" << endl
           << "bind (MYSQL_BIND*, id_image_type*, data_image_type&);"
           << endl;

        // grow()
        //
        os << "static bool" << endl
           << "grow (data_image_type&, my_bool*);"
           << endl;

        // init (data_image)
        //
        os << "static bool" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (data_image_type&, index_type, const value_type&);";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (data_image_type&, const key_type&, const value_type&);";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (data_image_type&, const value_type&);";
            break;
          }
        }

        os << endl;

        // init (data)
        //
        os << "static void" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (index_type&, value_type&, ";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (key_type&, value_type&, ";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (value_type&, ";
            break;
          }
        }

        os << "const data_image_type&, database&);"
           << endl;

        // insert_one
        //
        os << "static void" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "insert_one (index_type, const value_type&, void*);";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "insert_one (const key_type&, const value_type&, void*);";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "insert_one (const value_type&, void*);";
            break;
          }
        }

        os << endl;

        // load_all
        //
        os << "static bool" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "load_all (index_type&, value_type&, void*);";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "load_all (key_type&, value_type&, void*);";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "load_all (value_type&, void*);";
            break;
          }
        }

        os << endl;

        // delete_all
        //
        os << "static void" << endl
           << "delete_all (void*);"
           << endl;

        // persist
        //
        os << "static void" << endl
           << "persist (const container_type&," << endl
           << "id_image_type&," << endl
           << "bool," << endl
           << "statements_type&);"
           << endl;

        // load
        //
        os << "static void" << endl
           << "load (container_type&," << endl
           << "id_image_type&," << endl
           << "bool," << endl
           << "statements_type&);"
           << endl;

        // update
        //
        os << "static void" << endl
           << "update (const container_type&," << endl
           << "id_image_type&," << endl
           << "bool," << endl
           << "statements_type&);"
           << endl;

        // erase
        //
        os << "static void" << endl
           << "erase (id_image_type&, bool, statements_type&);"
           << endl;

        os << "};";
      }
    };

    //
    //
    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c), image_type_ (c), id_image_member_ (c, "id_")
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

        semantics::data_member& id (id_member (c));
        bool auto_id (id.count ("auto"));

        os << "// " << c.name () << endl
           << "//" << endl;

        os << "template <>" << endl
           << "class access::object_traits< " << type << " >"
           << "{"
           << "public:" << endl;

        // object_type & pointer_type
        //
        os << "typedef " << type << " object_type;"
           << "typedef " << c.get<string> ("object-pointer") << " pointer_type;";

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
        os << "struct id_image_type"
           << "{";

        id_image_member_.traverse (id);

        os << "};";

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

        // Statements.
        //
        os << "static const char* const persist_statement;"
           << "static const char* const find_statement;"
           << "static const char* const update_statement;"
           << "static const char* const erase_statement;";

        if (options.generate_query ())
          os << "static const char* const query_clause;";

        os << endl;

        //
        // Containers.
        //

        // Traits types.
        //
        {
          // @@ Make it class members?
          //
          container_traits t (*this);
          t.traverse (c);
        }

        // Statement cache (forward declaration).
        //
        os << "struct container_statement_cache_type;"
           << endl;

        //
        // Functions.
        //

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
           << "init (object_type&, const image_type&, database&);"
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
           << "find (mysql::object_statements<object_type>&," << endl
           << "const id_type&," << endl
           << "bool&);";

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
           << "init (value_type&, const image_type&, database&);"
           << endl;

        os << "};";
      }

    private:
      image_type image_type_;
      image_member id_image_member_;
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
      ctx.os << "#include <odb/result.hxx>" << endl;

    ctx.os << "#include <odb/container-traits.hxx>" << endl
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
