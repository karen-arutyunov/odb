// file      : odb/mysql/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <sstream>

#include <odb/mysql/common.hxx>
#include <odb/mysql/source.hxx>

using namespace std;

namespace mysql
{
  namespace
  {
    struct object_columns: object_columns_base, context
    {
      object_columns (context& c, char const* suffix = "")
          : object_columns_base (c),
            context (c),
            first_ (true),
            suffix_ (suffix)
      {
      }

      object_columns (context& c, bool first, char const* suffix = "")
          : object_columns_base (c),
            context (c),
            first_ (first),
            suffix_ (suffix)
      {
      }

      virtual void
      column (semantics::data_member&, string const& name, bool first)
      {
        if (!first || !first_)
          os << ",\"" << endl;

        os << "\"`" << name << "`" << suffix_;
      }

    private:
      bool first_;
      string suffix_;
    };

    const char* integer_buffer_types[] =
    {
      "MYSQL_TYPE_TINY",
      "MYSQL_TYPE_SHORT",
      "MYSQL_TYPE_LONG",     // *_bind_param() doesn't support INT24.
      "MYSQL_TYPE_LONG",
      "MYSQL_TYPE_LONGLONG"
    };

    const char* float_buffer_types[] =
    {
      "MYSQL_TYPE_FLOAT",
      "MYSQL_TYPE_DOUBLE"
    };

    const char* date_time_buffer_types[] =
    {
      "MYSQL_TYPE_DATE",
      "MYSQL_TYPE_TIME",
      "MYSQL_TYPE_DATETIME",
      "MYSQL_TYPE_TIMESTAMP",
      "MYSQL_TYPE_SHORT"
    };

    const char* char_bin_buffer_types[] =
    {
      "MYSQL_TYPE_STRING", // CHAR
      "MYSQL_TYPE_BLOB",   // BINARY,
      "MYSQL_TYPE_STRING", // VARCHAR
      "MYSQL_TYPE_BLOB",   // VARBINARY
      "MYSQL_TYPE_STRING", // TINYTEXT
      "MYSQL_TYPE_BLOB",   // TINYBLOB
      "MYSQL_TYPE_STRING", // TEXT
      "MYSQL_TYPE_BLOB",   // BLOB
      "MYSQL_TYPE_STRING", // MEDIUMTEXT
      "MYSQL_TYPE_BLOB",   // MEDIUMBLOB
      "MYSQL_TYPE_STRING", // LONGTEXT
      "MYSQL_TYPE_BLOB"    // LONGBLOB
    };

    //
    // bind
    //

    struct bind_member: member_base
    {
      bind_member (context& c,
                   size_t& index,
                   string const& var = string (),
                   string const& arg = string ())
          : member_base (c, var), index_ (index), arg_override_ (arg)
      {
      }

      bind_member (context& c,
                   size_t& index,
                   string const& var,
                   string const& arg,
                   semantics::type& t,
                   string const& fq_type,
                   string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix),
            index_ (index),
            arg_override_ (arg)
      {
      }

      virtual void
      pre (member_info& mi)
      {
        if (container (mi.t))
          return;

        ostringstream ostr;
        ostr << "b[" << index_ << "UL]";
        b = ostr.str ();

        arg = arg_override_.empty () ? string ("i") : arg_override_;

        if (var_override_.empty ())
          os << "// " << mi.m.name () << endl
             << "//" << endl;
      }

      virtual void
      post (member_info& mi)
      {
        if (container (mi.t))
          return;
        else if (semantics::class_* c = comp_value (mi.t))
          index_ += column_count (*c);
        else
          index_++;
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << "composite_value_traits< " << mi.fq_type () <<
          " >::bind (" << endl
           << "b + " << index_ << "UL, " << arg << "." << mi.var << "value);"
           << endl;
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        // While the is_unsigned should indicate whether the
        // buffer variable is unsigned, rather than whether the
        // database type is unsigned, in case of the image types,
        // this is the same.
        //
        os << b << ".buffer_type = " <<
          integer_buffer_types[mi.st->type - sql_type::TINYINT] << ";"
           << b << ".is_unsigned = " << (mi.st->unsign ? "1" : "0") << ";"
           << b << ".buffer = &" << arg << "." << mi.var << "value;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << b << ".buffer_type = " <<
          float_buffer_types[mi.st->type - sql_type::FLOAT] << ";"
           << b << ".buffer = &" << arg << "." << mi.var << "value;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        os << b << ".buffer_type = MYSQL_TYPE_NEWDECIMAL;"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << b << ".buffer_type = " <<
          date_time_buffer_types[mi.st->type - sql_type::DATE] << ";"
           << b << ".buffer = &" << arg << "." << mi.var << "value;";

        if (mi.st->type == sql_type::YEAR)
          os << b << ".is_unsigned = 0;";

        os << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // MySQL documentation is quite confusing about the use of
        // buffer_length and length when it comes to input parameters.
        // Source code, however, tells us that it uses buffer_length
        // only if length is NULL.
        //
        os << b << ".buffer_type = " <<
          char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << b << ".buffer_type = " <<
          char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Treated as a BLOB.
        //
        os << b << ".buffer_type = MYSQL_TYPE_BLOB;"
           << b << ".buffer = " << arg << "." << mi.var << "value;"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "sizeof (" << arg << "." << mi.var << "value));"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << b << ".buffer_type = MYSQL_TYPE_STRING;"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << b << ".buffer_type = MYSQL_TYPE_STRING;"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;"
           << endl;
      }

    private:
      size_t& index_;

      string b;
      string arg;
      string arg_override_;
    };

    struct bind_base: traversal::class_, context
    {
      bind_base (context& c, size_t& index)
          : context (c), index_ (index)
      {
      }

      virtual void
      traverse (type& c)
      {
        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "composite_value_traits< " << c.fq_name () <<
          " >::bind (b + " << index_ << "UL, i);"
           << endl;

        index_ += column_count (c);
      }

    private:
      size_t& index_;
    };

    //
    // grow
    //

    struct grow_member: member_base
    {
      grow_member (context& c, size_t& index)
          : member_base (c), index_ (index)
      {
      }

      grow_member (context& c,
                   size_t& index,
                   string const& var,
                   semantics::type& t,
                   string const& fq_type,
                   string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix), index_ (index)
      {
      }

      virtual void
      pre (member_info& mi)
      {
        if (container (mi.t))
          return;

        ostringstream ostr;
        ostr << "e[" << index_ << "UL]";
        e = ostr.str ();

        if (var_override_.empty ())
          os << "// " << mi.m.name () << endl
             << "//" << endl;
      }

      virtual void
      post (member_info& mi)
      {
        if (container (mi.t))
          return;
        else if (semantics::class_* c = comp_value (mi.t))
          index_ += column_count (*c);
        else
          index_++;
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << "if (composite_value_traits< " << mi.fq_type () <<
          " >::grow (" << endl
           << "i." << mi.var << "value, e + " << index_ << "UL))"
           << "{"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_integer (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_float (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        // @@ Optimization disabled.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_date_time (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // @@ Optimization disabled.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_bit (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

    private:
      string e;
      size_t& index_;
    };

    struct grow_base: traversal::class_, context
    {
      grow_base (context& c, size_t& index)
          : context (c), index_ (index)
      {
      }

      virtual void
      traverse (type& c)
      {
        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "if (composite_value_traits< " << c.fq_name () <<
          " >::grow (i, e + " << index_ << "UL))"
           << "{"
           << "grew = true;"
           << "}";

        index_ += column_count (c);
      }

    private:
      size_t& index_;
    };

    //
    // init image
    //

    struct init_image_member: member_base
    {
      init_image_member (context& c,
                         string const& var = string (),
                         string const& member = string ())
          : member_base (c, var),
            member_image_type_ (c),
            member_database_type_ (c),
            member_override_ (member)
      {
      }

      init_image_member (context& c,
                         string const& var,
                         string const& member,
                         semantics::type& t,
                         string const& fq_type,
                         string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix),
            member_image_type_ (c, t, fq_type, key_prefix),
            member_database_type_ (c, t, fq_type, key_prefix),
            member_override_ (member)
      {
      }

      virtual void
      pre (member_info& mi)
      {
        if (container (mi.t))
          return;

        if (!member_override_.empty ())
          member = member_override_;
        else
        {
          string const& name (mi.m.name ());
          member = "o." + name;

          os << "// " << name << endl
             << "//" << endl;
        }

        if (comp_value (mi.t))
          traits = "composite_value_traits< " + mi.fq_type () + " >";
        else
        {
          if (semantics::class_* c = object_pointer (mi.m, key_prefix_))
          {
            type = "obj_traits::id_type";
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);

            // Handle NULL pointers and extract the id.
            //
            os << "{"
               << "typedef object_traits< " << c->fq_name () <<
              " > obj_traits;"
               << "typedef pointer_traits< " << mi.fq_type () <<
              " > ptr_traits;"
               << endl
               << "bool is_null (ptr_traits::null_ptr (" << member << "));"
               << "if (!is_null)"
               << "{"
               << "const " << type << "& id (" << endl
               << "obj_traits::id (ptr_traits::get_ref (" << member << ")));"
               << endl;

            member = "id";
          }
          else
          {
            type = mi.fq_type ();
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);

            os << "{"
               << "bool is_null;";
          }

          traits = "mysql::value_traits<\n    "
            + type + ",\n    "
            + image_type + ",\n    "
            + db_type + " >";
        }
      }

      virtual void
      post (member_info& mi)
      {
        if (container (mi.t))
          return;

        if (!comp_value (mi.t))
        {
          if (object_pointer (mi.m, key_prefix_))
            os << "}";

          os << "i." << mi.var << "null = is_null;"
             << "}";
        }
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << "if (" << traits << "::init (i." << mi.var << "value, " <<
          member << "))"
           << "{"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        os << traits << "::set_image (" << endl
           << "i." << mi.var << "value, is_null, " << member << ");";
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << traits << "::set_image (" << endl
           << "i." << mi.var << "value, is_null, " << member << ");";
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        // @@ Optimization: can remove growth check if buffer is fixed.
        //
        os << "std::size_t size;"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << traits << "::set_image (" << endl
           << "i." << mi.var << "value, is_null, " << member << ");";
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // @@ Optimization: can remove growth check if buffer is fixed.
        //
        os << "std::size_t size;"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << "std::size_t size;"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Represented as a BLOB.
        //
        os << "std::size_t size;"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "sizeof (i." << mi.var << "value)," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);";
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << "std::size_t size;"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << "std::size_t size;"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

    private:
      string type;
      string db_type;
      string member;
      string image_type;
      string traits;

      member_image_type member_image_type_;
      member_database_type member_database_type_;

      string member_override_;
    };

    struct init_image_base: traversal::class_, context
    {
      init_image_base (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "if (composite_value_traits< " << c.fq_name () <<
          " >::init (i, o))"
           << "{"
           << "grew = true;"
           << "}";
      }
    };

    //
    // init value
    //

    struct init_value_member: member_base
    {
      init_value_member (context& c)
          : member_base (c),
            member_image_type_ (c),
            member_database_type_ (c)
      {
      }

      init_value_member (context& c,
                         string const& var,
                         string const& member,
                         semantics::type& t,
                         string const& fq_type,
                         string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix),
            member_image_type_ (c, t, fq_type, key_prefix),
            member_database_type_ (c, t, fq_type, key_prefix),
            member_override_ (member)
      {
      }

      virtual void
      pre (member_info& mi)
      {
        if (container (mi.t))
          return;

        if (!member_override_.empty ())
          member = member_override_;
        else
        {
          string const& name (mi.m.name ());
          member = "o." + name;

          os << "// " << name << endl
             << "//" << endl;
        }

        if (comp_value (mi.t))
          traits = "composite_value_traits< " + mi.fq_type () + " >";
        else
        {
          if (semantics::class_* c = object_pointer (mi.m, key_prefix_))
          {
            type = "obj_traits::id_type";
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);

            // Handle NULL pointers and extract the id.
            //
            os << "{"
               << "typedef object_traits< " << c->fq_name () <<
              " > obj_traits;"
               << "typedef pointer_traits< " << mi.fq_type () <<
              " > ptr_traits;"
               << endl
               << "if (i." << mi.var << "null)" << endl
               << member << " = ptr_traits::pointer_type ();"
               << "else"
               << "{"
               << type << " id;";

            member = "id";
          }
          else
          {
            type = mi.fq_type ();
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);
          }

          traits = "mysql::value_traits<\n    "
            + type + ",\n    "
            + image_type + ",\n    "
            + db_type + " >";
        }
      }

      virtual void
      post (member_info& mi)
      {
        if (container (mi.t))
          return;

        if (!comp_value (mi.t) && object_pointer (mi.m, key_prefix_))
        {
          member = member_override_.empty ()
            ? "o." + mi.m.name ()
            : member_override_;

          os << "// If a compiler error points to the line below, then" << endl
             << "// it most likely means that a pointer used in a member" << endl
             << "// cannot be initialized from an object pointer." << endl
             << "//" << endl
             << member << " = ptr_traits::pointer_type (" << endl
             << "db.load< obj_traits::object_type > (id));"
             << "}"
             << "}";
        }
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << traits << "::init (" << member << ", i." <<
          mi.var << "value, db);"
           << endl;
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << ", i." << mi.var << "value, " <<
          "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << ", i." << mi.var << "value, " <<
          "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << ", i." << mi.var << "value, " <<
          "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Represented as a BLOB.
        //
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

    private:
      string type;
      string db_type;
      string image_type;
      string traits;
      string member;

      member_image_type member_image_type_;
      member_database_type member_database_type_;

      string member_override_;
    };

    struct init_value_base: traversal::class_, context
    {
      init_value_base (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "composite_value_traits< " << c.fq_name () <<
          " >::init (o, i, db);"
           << endl;
      }
    };

    // Member-specific traits types for container members.
    //
    struct container_traits: object_members_base, context
    {
      container_traits (context& c, semantics::class_& obj)
          : object_members_base (c, true, true),
            context (c),
            object_ (obj),
            id_member_ (id_member (obj))
      {
        obj_scope_ = "access::object_traits< " + obj.fq_name () + " >";
      }

      virtual void
      container (semantics::data_member& m)
      {
        using semantics::type;

        type& t (m.type ());
        container_kind_type ck (container_kind (t));

        type& vt (container_vt (t));
        type* it (0);
        type* kt (0);

        bool grow (false);

        switch (ck)
        {
        case ck_ordered:
          {
            it = &container_it (t);
            grow = grow || context::grow (m, *it, "index");
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            kt = &container_kt (t);
            grow = grow || context::grow (m, *kt, "key");
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        grow = grow || context::grow (m, vt, "value");

        bool eager_ptr (is_a (m, eager_pointer, vt, "value") ||
                        has_a (vt, eager_pointer));

        string name (prefix_ + public_name (m) + "_traits");
        string scope (obj_scope_ + "::" + name);

        os << "// " << m.name () << endl
           << "//" << endl
           << endl;

        //
        // Statements.
        //
        string table (table_name (m, table_prefix_));

        // insert_one_statement
        //
        os << "const char* const " << scope << "::insert_one_statement =" << endl
           << "\"INSERT INTO `" << table << "` (\"" << endl
           << "\"`" << column_name (m, "id", "object_id") << "`";

        switch (ck)
        {
        case ck_ordered:
          {
            os << ",\"" << endl
               << "\"`" << column_name (m, "index", "index") << "`";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            if (comp_value (*kt))
            {
              object_columns t (*this, false);
              t.traverse_composite (m, *kt, "key", "key");
            }
            else
            {
              os << ",\"" << endl
                 << "\"`" << column_name (m, "key", "key") << "`";
            }
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        if (comp_value (vt))
        {
          object_columns t (*this, false);
          t.traverse_composite (m, vt, "value", "value");
        }
        else
        {
          os << ",\"" << endl
             << "\"`" << column_name (m, "value", "value") << "`";
        }

        os << "\"" << endl
           << "\") VALUES (";

        for (size_t i (0), n (m.get<size_t> ("data-column-count")); i < n; ++i)
          os << (i != 0 ? "," : "") << '?';

        os << ")\";"
           << endl;

        // select_all_statement
        //
        os << "const char* const " << scope << "::select_all_statement =" << endl
           << "\"SELECT \"" << endl
           << "\"`" << column_name (m, "id", "object_id") << "`";

        switch (ck)
        {
        case ck_ordered:
          {
            os << ",\"" << endl
               << "\"`" << column_name (m, "index", "index") << "`";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            if (comp_value (*kt))
            {
              object_columns t (*this, false);
              t.traverse_composite (m, *kt, "key", "key");
            }
            else
            {
              os << ",\"" << endl
                 << "\"`" << column_name (m, "key", "key") << "`";
            }
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        if (comp_value (vt))
        {
          object_columns t (*this, false);
          t.traverse_composite (m, vt, "value", "value");
        }
        else
        {
          os << ",\"" << endl
             << "\"`" << column_name (m, "value", "value") << "`";
        }

        os << "\"" << endl
           << "\" FROM `" << table << "` WHERE `" <<
          column_name (m, "id", "object_id") << "` = ?\"" << endl;

        if (ck == ck_ordered)
          os << "\" ORDER BY `" << column_name (m, "index", "index") << "`\"";

        os << ";"
           << endl;

        // delete_all_statement
        //
        os << "const char* const " << scope << "::delete_all_statement =" << endl
           << "\"DELETE FROM `" << table << "`\"" << endl
           << "\" WHERE `" << column_name (m, "id", "object_id") << "` = ?\";"
           << endl;

        //
        // Functions.
        //

        // bind()
        //
        {
          size_t index;
          bind_member bind_id (*this, index, "id_", "id");

          // bind (cond_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (MYSQL_BIND* b, id_image_type* p, cond_image_type& c)"
             << "{";

          index = 0;

          os << "// object_id" << endl
             << "//" << endl
             << "if (p != 0)"
             << "{"
             << "id_image_type& id (*p);";
          bind_id.traverse (id_member_);
          os << "}";

          switch (ck)
          {
          case ck_ordered:
            {
              os << "// index" << endl
                 << "//" << endl;
              bind_member bm (
                *this, index, "index_", "c", *it, "index_type", "index");
              bm.traverse (m);
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              bind_member bm (
                *this, index, "key_", "c", *kt, "key_type", "key");
              bm.traverse (m);
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "// value" << endl
                 << "//" << endl;
              bind_member bm (
                *this, index, "value_", "c", vt, "value_type", "value");
              bm.traverse (m);
              break;
            }
          }

          os << "}";

          // bind (data_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (MYSQL_BIND* b, id_image_type* p, data_image_type& d)"
             << "{";

          index = 0;

          os << "// object_id" << endl
             << "//" << endl
             << "if (p != 0)"
             << "{"
             << "id_image_type& id (*p);";
          bind_id.traverse (id_member_);
          os << "}";

          switch (ck)
          {
          case ck_ordered:
            {
              os << "// index" << endl
                 << "//" << endl;
              bind_member bm (
                *this, index, "index_", "d", *it, "index_type", "index");
              bm.traverse (m);
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              bind_member bm (
                *this, index, "key_", "d", *kt, "key_type", "key");
              bm.traverse (m);
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
          bind_member bm (
            *this, index, "value_", "d", vt, "value_type", "value");
          bm.traverse (m);

          os << "}";
        }

        // grow ()
        //
        {
          size_t index (0);

          os << "void " << scope << "::" << endl
             << "grow (data_image_type& i, my_bool* e)"
             << "{"
             << "bool grew (false);"
             << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              os << "// index" << endl
                 << "//" << endl;
              grow_member gm (
                *this, index, "index_", *it, "index_type", "index");
              gm.traverse (m);
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              grow_member gm (*this, index, "key_", *kt, "key_type", "key");
              gm.traverse (m);
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
          grow_member gm (*this, index, "value_", vt, "value_type", "value");
          gm.traverse (m);

          os << "if (grew)" << endl
             << "i.version++;"
             << "}";
        }

        // init (data_image)
        //
        os << "void " << scope << "::" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (data_image_type& i, index_type j, const value_type& v)"
               << "{"
               << "bool grew (false);"
               << endl
               << "// index" << endl
               << "//" << endl;

            init_image_member im (
              *this, "index_", "j", *it, "index_type", "index");
            im.traverse (m);

            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (data_image_type& i, const key_type& k, " <<
              "const value_type& v)"
               << "{"
               << "bool grew (false);"
               << endl
               << "// key" << endl
               << "//" << endl;

            init_image_member im (*this, "key_", "k", *kt, "key_type", "key");
            im.traverse (m);

            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (data_image_type& i, const value_type& v)"
               << "{"
               << "bool grew (false);"
               << endl;
            break;
          }
        }

        os << "// value" << endl
           << "//" << endl;
        {
          init_image_member im (
            *this, "value_", "v", vt, "value_type", "value");
          im.traverse (m);
        }

        os << "if (grew)" << endl
           << "i.version++;"
           << "}";

        // init (data)
        //
        os << "void " << scope << "::" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (index_type& j, value_type& v, " <<
              "const data_image_type& i, database& db)"
               << "{"
               << "ODB_POTENTIALLY_UNUSED (db);"
               << endl
               << "// index" << endl
               << "//" << endl;

            init_value_member im (
              *this, "index_", "j", *it, "index_type", "index");
            im.traverse (m);

            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (key_type& k, value_type& v, " <<
              "const data_image_type& i, database& db)"
               << "{"
               << "ODB_POTENTIALLY_UNUSED (db);"
               << endl
               << "// key" << endl
               << "//" << endl;

            init_value_member im (*this, "key_", "k", *kt, "key_type", "key");
            im.traverse (m);

            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (value_type& v, const data_image_type& i, " <<
              "database& db)"
               << "{"
               << "ODB_POTENTIALLY_UNUSED (db);"
               << endl;
            break;
          }
        }

        os << "// value" << endl
           << "//" << endl;
        {
          // If the value is an object pointer, pass the id type as a
          // type override.
          //
          init_value_member im (
            *this, "value_", "v", vt, "value_type", "value");
          im.traverse (m);
        }
        os << "}";

        // insert_one
        //
        os << "void " << scope << "::" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "insert_one (index_type i, const value_type& v, void* d)";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "insert_one (const key_type& k, const value_type& v, void* d)";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "insert_one (const value_type& v, void* d)";
            break;
          }
        }

        os << "{"
           << "using namespace mysql;"
           << endl
           << "typedef container_statements< " << name << " > statements;"
           << "statements& sts (*static_cast< statements* > (d));"
           << "binding& b (sts.data_image_binding ());"
           << "data_image_type& di (sts.data_image ());"
           << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (di, i, v);";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (di, k, v);";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (di, v);";
            break;
          }
        }

        os << endl
           << "if (di.version != sts.data_image_version () ||  b.version == 0)"
           << "{"
           << "bind (b.bind, 0, di);"
           << "sts.data_image_version (di.version);"
           << "b.version++;"
           << "}"
           << "if (!sts.insert_one_statement ().execute ())" << endl
           << "throw object_already_persistent ();"
           << "}";


        // load_all
        //
        os << "bool " << scope << "::" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "load_all (index_type& i, value_type& v, void* d)";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "load_all (key_type& k, value_type& v, void* d)";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "load_all (value_type& v, void* d)";
            break;
          }
        }

        os << "{"
           << "using namespace mysql;"
           << endl
           << "typedef container_statements< " << name << " > statements;"
           << "statements& sts (*static_cast< statements* > (d));"
           << "data_image_type& di (sts.data_image ());";

        // Extract current element.
        //
        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (i, v, di, sts.connection ().database ());"
               << endl;
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (k, v, di, sts.connection ().database ());"
               << endl;
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (v, di, sts.connection ().database ());"
               << endl;
            break;
          }
        }

        // If we are loading an eager pointer, then the call to init
        // above executed other statements which potentially could
        // change the image.
        //
        if (eager_ptr)
        {
          os << "id_image_type& ii (sts.id_image ());"
             << endl
             << "if (di.version != sts.data_image_version () ||" << endl
             << "ii.version != sts.data_id_image_version ())"
             << "{"
             << "binding& b (sts.data_image_binding ());"
             << "bind (b.bind, &ii, di);"
             << "sts.data_image_version (di.version);"
             << "sts.data_id_image_version (ii.version);"
             << "b.version++;"
             << "}";
        }

        // Fetch next.
        //
        os << "select_statement& st (sts.select_all_statement ());"
           << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "grow (di, sts.data_image_error ());"
             << endl
             << "if (di.version != sts.data_image_version ())"
             << "{"
             << "binding& b (sts.data_image_binding ());"
             << "bind (b.bind, 0, di);"
             << "sts.data_image_version (di.version);"
             << "b.version++;"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "if (r == select_statement::no_data)"
           << "{"
           << "st.free_result ();"
           << "return false;"
           << "}"
           << "return true;"
           << "}";

        // delete_all
        //
        os << "void " << scope << "::" << endl
           << "delete_all (void* d)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "typedef container_statements< " << name << " > statements;"
           << "statements& sts (*static_cast< statements* > (d));"
           << "sts.delete_all_statement ().execute ();"
           << "}";

        // persist
        //
        os << "void " << scope << "::" << endl
           << "persist (const container_type& c," << endl
           << "id_image_type& id," << endl
           << "statements_type& sts)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "binding& b (sts.data_image_binding ());"
           << "if (id.version != sts.data_id_image_version () || b.version == 0)"
           << "{"
           << "bind (b.bind, &id, sts.data_image ());"
           << "sts.data_id_image_version (id.version);"
           << "b.version++;"
           << "}"
           << "sts.id_image (id);"
           << "container_traits::persist (c, sts.functions ());"
           << "}";

        // load
        //
        os << "void " << scope << "::" << endl
           << "load (container_type& c," << endl
           << "id_image_type& id," << endl
           << "statements_type& sts)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "binding& db (sts.data_image_binding ());"
           << "if (id.version != sts.data_id_image_version () || db.version == 0)"
           << "{"
           << "bind (db.bind, &id, sts.data_image ());"
           << "sts.data_id_image_version (id.version);"
           << "db.version++;"
           << "}"
           << "binding& cb (sts.cond_image_binding ());"
           << "if (id.version != sts.cond_id_image_version () || cb.version == 0)"
           << "{"
           << "bind (cb.bind, &id, sts.cond_image ());"
           << "sts.cond_id_image_version (id.version);"
           << "cb.version++;"
           << "}"
           << "select_statement& st (sts.select_all_statement ());"
           << "st.execute ();";

        // If we are loading eager object pointers, cache the result
        // since we will be loading other objects.
        //
        if (eager_ptr)
          os << "st.cache ();";

        os << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "data_image_type& di (sts.data_image ());"
             << "grow (di, sts.data_image_error ());"
             << endl
             << "if (di.version != sts.data_image_version ())"
             << "{"
             << "bind (db.bind, 0, sts.data_image ());"
             << "sts.data_image_version (di.version);"
             << "db.version++;"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "bool more (r != select_statement::no_data);"
           << endl
           << "if (!more)" << endl
           << "st.free_result ();"
           << endl
           << "sts.id_image (id);"
           << "container_traits::load (c, more, sts.functions ());"
           << "}";

        // update
        //
        os << "void " << scope << "::" << endl
           << "update (const container_type& c," << endl
           << "id_image_type& id," << endl
           << "statements_type& sts)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "binding& db (sts.data_image_binding ());"
           << "if (id.version != sts.data_id_image_version () || db.version == 0)"
           << "{"
           << "bind (db.bind, &id, sts.data_image ());"
           << "sts.data_id_image_version (id.version);"
           << "db.version++;"
           << "}"
          //
          // We may need cond if the specialization calls delete_all.
          //
           << "binding& cb (sts.cond_image_binding ());"
           << "if (id.version != sts.cond_id_image_version () || cb.version == 0)"
           << "{"
           << "bind (cb.bind, &id, sts.cond_image ());"
           << "sts.cond_id_image_version (id.version);"
           << "cb.version++;"
           << "}"
           << "sts.id_image (id);"
           << "container_traits::update (c, sts.functions ());"
           << "}";

        // erase
        //
        os << "void " << scope << "::" << endl
           << "erase (id_image_type& id, statements_type& sts)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "binding& b (sts.cond_image_binding ());"
           << "if (id.version != sts.cond_id_image_version () || b.version == 0)"
           << "{"
           << "bind (b.bind, &id, sts.cond_image ());"
           << "sts.cond_id_image_version (id.version);"
           << "b.version++;"
           << "}"
           << "sts.id_image (id);"
           << "container_traits::erase (sts.functions ());"
           << "}";
      }

    private:
      string obj_scope_;
      semantics::class_& object_;
      semantics::data_member& id_member_;
    };

    // Container statement cache members.
    //
    struct container_cache_members: object_members_base, context
    {
      container_cache_members (context& c)
          : object_members_base (c, true, false),
            context (c),
            containers_ (false)
      {
      }

      bool
      containers () const
      {
        return containers_;
      }

      virtual void
      container (semantics::data_member& m)
      {
        string traits (prefix_ + public_name (m) + "_traits");
        os << "mysql::container_statements< " << traits << " > " <<
          prefix_ << m.name () << ";";

        containers_ = true;
      }

    private:
      bool containers_;
    };

    struct container_cache_init_members: object_members_base, context
    {
      container_cache_init_members (context& c)
          : object_members_base (c, true, false), context (c), first_ (true)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        if (first_)
        {
          os << endl
             << ": ";
          first_ = false;
        }
        else
          os << "," << endl
             << "  ";

        os << prefix_ << m.name () << " (c)";
      }

    private:
      bool first_;
    };

    // Calls for container members.
    //
    struct container_calls: object_members_base, context
    {
      enum call_type
      {
        persist_call,
        load_call,
        update_call,
        erase_call
      };

      container_calls (context& c, call_type call)
          : object_members_base (c, true, false), context (c), call_ (call)
      {
      }

      virtual void
      composite (semantics::data_member& m, semantics::type& t)
      {
        string old (obj_prefix_);
        obj_prefix_ += m.name ();
        obj_prefix_ += '.';
        object_members_base::composite (m, t);
        obj_prefix_ = old;
      }

      virtual void
      container (semantics::data_member& m)
      {
        using semantics::type;

        string const& name (m.name ());
        string obj_name (obj_prefix_ + name);
        string sts_name (prefix_ + name);
        string traits (prefix_ + public_name (m) + "_traits");

        switch (call_)
        {
        case persist_call:
          {
            os << traits << "::persist (obj." << obj_name << ", i, " <<
              "sts.container_statment_cache ()." << sts_name << ");";
            break;
          }
        case load_call:
          {
            os << traits << "::load (obj." << obj_name << ", i, " <<
              "sts.container_statment_cache ()." << sts_name << ");";
            break;
          }
        case update_call:
          {
            os << traits << "::update (obj." << obj_name << ", i, " <<
              "sts.container_statment_cache ()." << sts_name << ");";
            break;
          }
        case erase_call:
          {
            os << traits << "::erase (i, sts.container_statment_cache ()." <<
              sts_name << ");";
            break;
          }
        }
      }

    private:
      call_type call_;
      string obj_prefix_;
    };

    //
    //
    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c),
            grow_base_ (c, index_),
            grow_member_ (c, index_),
            bind_base_ (c, index_),
            bind_member_ (c, index_),
            bind_id_member_ (c, index_, "id_"),
            init_image_base_ (c),
            init_image_member_ (c),
            init_id_image_member_ (c, "id_", "id"),
            init_value_base_ (c),
            init_value_member_ (c)
      {
        grow_base_inherits_ >> grow_base_;
        grow_member_names_ >> grow_member_;

        bind_base_inherits_ >> bind_base_;
        bind_member_names_ >> bind_member_;

        init_image_base_inherits_ >> init_image_base_;
        init_image_member_names_ >> init_image_member_;

        init_value_base_inherits_ >> init_value_base_;
        init_value_member_names_ >> init_value_member_;
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
        string traits ("access::object_traits< " + type + " >");

        bool grow (context::grow (c));
        bool def_ctor (TYPE_HAS_DEFAULT_CONSTRUCTOR (c.tree_node ()));

        semantics::data_member& id (id_member (c));
        bool auto_id (id.count ("auto"));
        bool grow_id (context::grow (id));

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        //
        // Containers.
        //

        // Statement cache (definition).
        //
        bool containers;
        {
          os << "struct " << traits << "::container_statement_cache_type"
             << "{";

          container_cache_members cm (*this);
          cm.traverse (c);
          containers = cm.containers ();

          os << (containers ? "\n" : "")
             << "container_statement_cache_type (mysql::connection&" <<
            (containers ? " c" : "") << ")";

          container_cache_init_members im (*this);
          im.traverse (c);

          os << "{"
             << "}"
             << "};";
        }

        // Traits types.
        //
        if (containers)
        {
          container_traits t (*this, c);
          t.traverse (c);
        }

        // query columns
        //
        if (options.generate_query ())
        {
          query_columns t (*this, c);
          t.traverse (c);
        }

        // persist_statement
        //
        os << "const char* const " << traits << "::persist_statement =" << endl
           << "\"INSERT INTO `" << table_name (c) << "` (\"" << endl;

        {
          object_columns t (*this);
          t.traverse (c);
        }

        os << "\"" << endl
           << "\") VALUES (";

        for (size_t i (0), n (column_count (c)); i < n; ++i)
          os << (i != 0 ? "," : "") << '?';

        os << ")\";"
           << endl;

        // find_statement
        //
        os << "const char* const " << traits << "::find_statement =" << endl
           << "\"SELECT \"" << endl;

        {
          object_columns t (*this);
          t.traverse (c);
        }

        os << "\"" << endl
           << "\" FROM `" << table_name (c) << "` WHERE `" <<
          column_name (id) << "` = ?\";"
           << endl;

        // update_statement
        //
        os << "const char* const " << traits << "::update_statement =" << endl
           << "\"UPDATE `" << table_name (c) << "` SET \"" << endl;

        {
          object_columns t (*this, " = ?");
          t.traverse (c);
        }

        os << "\"" << endl
           << "\" WHERE `" << column_name (id) << "` = ?\";"
           << endl;

        // erase_statement
        //
        os << "const char* const " << traits << "::erase_statement =" << endl
           << "\"DELETE FROM `" << table_name (c) << "`\"" << endl
           << "\" WHERE `" << column_name (id) << "` = ?\";"
           << endl;

        // query_clause
        //
        if (options.generate_query ())
        {
          os << "const char* const " << traits << "::query_clause =" << endl
             << "\"SELECT \"" << endl;

          {
            object_columns t (*this);
            t.traverse (c);
          }

          os << "\"" << endl
             << "\" FROM `" << table_name (c) << "` \";"
             << endl;
        }

        // grow ()
        //
        os << "void " << traits << "::" << endl
           << "grow (image_type& i, my_bool* e)"
           << "{"
           << "bool grew (false);"
           << endl;

        index_ = 0;
        inherits (c, grow_base_inherits_);
        names (c, grow_member_names_);

        os << "if (grew)" << endl
           << "i.version++;" << endl
           << "}";

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND* b, image_type& i)"
           << "{";

        index_ = 0;
        inherits (c, bind_base_inherits_);
        names (c, bind_member_names_);

        os << "}";

        // bind (id_image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND* b, id_image_type& i)"
           << "{";

        index_ = 0;
        bind_id_member_.traverse (id);

        os << "}";

        // init (image, object)
        //
        os << "void " << traits << "::" << endl
           << "init (image_type& i, const object_type& o)"
           << "{"
           << "bool grew (false);"
           << endl;

        inherits (c, init_image_base_inherits_);
        names (c, init_image_member_names_);

        os << "if (grew)" << endl
           << "i.version++;" << endl
           << "}";

        // init (object, image)
        //
        os << "void " << traits << "::" << endl
           << "init (object_type& o, const image_type& i, database& db)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << endl;

        inherits (c, init_value_base_inherits_);
        names (c, init_value_member_names_);

        os << "}";

        // persist ()
        //
        os << "void " << traits << "::" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << "image_type& im (sts.image ());"
           << "binding& imb (sts.image_binding ());"
           << endl;

        if (auto_id)
          os << "obj." << id.name () << " = 0;";

        os << "init (im, obj);"
           << endl
           << "if (im.version != sts.image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im);"
           << "sts.image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "insert_statement& st (sts.persist_statement ());"
           << "if (!st.execute ())" << endl
           << "throw object_already_persistent ();"
           << endl;

        if (auto_id)
          os << "obj." << id.name () << " = static_cast<id_type> (st.id ());"
             << endl;

        if (containers)
        {
          os << "{";

          // Initialize id_image.
          //
          if (grow_id)
            os << "bool grew (false);";

          os << "const id_type& id (obj." << id.name () << ");"
             << "id_image_type& i (sts.id_image ());";
          init_id_image_member_.traverse (id);

          if (grow_id)
            os << "if (grew)" << endl
               << "i.version++;"
               << endl;

          container_calls t (*this, container_calls::persist_call);
          t.traverse (c);

          os << "}";
        }

        os << "}";

        // update ()
        //
        os << "void " << traits << "::" << endl
           << "update (database&, const object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl;

        // Initialize id image.
        //
        if (grow_id)
          os << "bool grew (false);";

        os << "const id_type& id (obj." << id.name () << ");"
           << "id_image_type& i (sts.id_image ());";
        init_id_image_member_.traverse (id);

        if (grow_id)
          os << "if (grew)" << endl
             << "i.version++;"
             << endl;

        os << "binding& idb (sts.id_image_binding ());"
           << "if (i.version != sts.id_image_version () || idb.version == 0)"
           << "{"
           << "bind (idb.bind, i);"
           << "sts.id_image_version (i.version);"
           << "idb.version++;"
           << "}";

        // Initialize data image.
        //
        os << "image_type& im (sts.image ());"
           << "binding& imb (sts.image_binding ());"
           << "init (im, obj);"
           << endl
           << "if (im.version != sts.image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im);"
           << "sts.image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "sts.update_statement ().execute ();";

          if (containers)
          {
            os << endl;
            container_calls t (*this, container_calls::update_call);
            t.traverse (c);
          }

        os << "}";

        // erase ()
        //
        os << "void " << traits << "::" << endl
           << "erase (database&, const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl;

        // Initialize id image.
        //
        if (grow_id)
          os << "bool grew (false);";

        os << "id_image_type& i (sts.id_image ());";
        init_id_image_member_.traverse (id);

        if (grow_id)
          os << "if (grew)" << endl
             << "i.version++;"
             << endl;

        os << "binding& idb (sts.id_image_binding ());"
           << "if (i.version != sts.id_image_version () || idb.version == 0)"
           << "{"
           << "bind (idb.bind, i);"
           << "sts.id_image_version (i.version);"
           << "idb.version++;"
           << "}"
           << "if (sts.erase_statement ().execute () != 1)" << endl
           << "throw object_not_persistent ();";

        if (containers)
        {
          os << endl;
          container_calls t (*this, container_calls::erase_call);
          t.traverse (c);
        }

        os << "}";

        // find ()
        //
        if (def_ctor)
        {
          os << traits << "::pointer_type" << endl
             << traits << "::" << endl
             << "find (database& db, const id_type& id)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "connection& conn (mysql::transaction::current ().connection ());"
             << "object_statements<object_type>& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << endl
             << "if (find (sts, id))"
             << "{"
             << "pointer_type p (access::object_factory< object_type, " <<
            "pointer_type  >::create ());"
             << "pointer_traits< pointer_type >::guard_type g (p);"
             << "object_type& obj (pointer_traits< pointer_type >::get_ref (p));"
             << "init (obj, sts.image (), db);";

          if (containers)
          {
            os << endl
               << "id_image_type& i (sts.id_image ());";
            container_calls t (*this, container_calls::load_call);
            t.traverse (c);
            os << endl;
          }

          os << "g.release ();"
             << "return p;"
             << "}"
             << "return pointer_type ();"
             << "}";
        }

        os << "bool " << traits << "::" << endl
           << "find (database& db, const id_type& id, object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl
           << "if (find (sts, id))"
           << "{"
           << "init (obj, sts.image (), db);";

        if (containers)
        {
          os << endl
             << "id_image_type& i (sts.id_image ());";
          container_calls t (*this, container_calls::load_call);
          t.traverse (c);
          os << endl;
        }

        os << "return true;"
           << "}"
           << "return false;"
           << "}";

        os << "bool " << traits << "::" << endl
           << "find (mysql::object_statements<object_type>& sts, " <<
          "const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl;

        // Initialize id image.
        //
        if (grow_id)
          os << "bool grew (false);";

        os << "id_image_type& i (sts.id_image ());";
        init_id_image_member_.traverse (id);

        if (grow_id)
          os << "if (grew)" << endl
             << "i.version++;"
             << endl;

        os << "binding& idb (sts.id_image_binding ());"
           << "if (i.version != sts.id_image_version () || idb.version == 0)"
           << "{"
           << "bind (idb.bind, i);"
           << "sts.id_image_version (i.version);"
           << "idb.version++;"
           << "}";

        // Rebind data image.
        //
        os << "image_type& im (sts.image ());"
           << "binding& imb (sts.image_binding ());"
           << endl
           << "if (im.version != sts.image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im);"
           << "sts.image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "select_statement& st (sts.find_statement ());"
           << "st.execute ();"
           << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "grow (im, sts.image_error ());"
             << endl
             << "if (im.version != sts.image_version ())"
             << "{"
             << "bind (imb.bind, im);"
             << "sts.image_version (im.version);"
             << "imb.version++;"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "st.free_result ();"
           << "return r != select_statement::no_data;"
           << "}";

        // query ()
        //
        if (options.generate_query ())
        {
          os << "result< " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query (database&, const query_type& q)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "connection& conn (mysql::transaction::current ().connection ());"
             << "object_statements<object_type>& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << endl
             << "image_type& im (sts.image ());"
             << "binding& imb (sts.image_binding ());"
             << endl
             << "if (im.version != sts.image_version () || imb.version == 0)"
             << "{"
             << "bind (imb.bind, im);"
             << "sts.image_version (im.version);"
             << "imb.version++;"
             << "}"
             << "details::shared_ptr<select_statement> st (" << endl
             << "new (details::shared) select_statement (conn," << endl
             << "query_clause + q.clause ()," << endl
             << "q.parameters ()," << endl
             << "imb));"
             << "st->execute ();"
             << endl
             << "details::shared_ptr<odb::result_impl<object_type> > r (" << endl
             << "new (details::shared) mysql::result_impl<object_type> (st, sts));"
             << "return result<object_type> (r);"
             << "}";
        }
      }

      virtual void
      traverse_value (type& c)
      {
        bool columns (column_count (c) != 0);

        string const& type (c.fq_name ());
        string traits ("access::composite_value_traits< " + type + " >");

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        // grow ()
        //
        os << "bool " << traits << "::" << endl
           << "grow (image_type& i, my_bool*" << (columns ? " e" : "") << ")"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << endl
           << "bool grew (false);"
           << endl;

        index_ = 0;
        inherits (c, grow_base_inherits_);
        names (c, grow_member_names_);

        os << "return grew;"
           << "}";

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND*" << (columns ? " b" : "") << ", " <<
          "image_type&" << (columns ? " i" : "") << ")"
           << "{";

        index_ = 0;
        inherits (c, bind_base_inherits_);
        names (c, bind_member_names_);

        os << "}";

        // init (image, object)
        //
        os << "bool " << traits << "::" << endl
           << "init (image_type&" << (columns ? " i" : "") << ", " <<
          "const value_type&" << (columns ? " o" : "") << ")"
           << "{"
           << "bool grew (false);"
           << endl;

        inherits (c, init_image_base_inherits_);
        names (c, init_image_member_names_);

        os << "return grew;"
           << "}";

        // init (object, image)
        //
        os << "void " << traits << "::" << endl
           << "init (value_type&" << (columns ? " o" : "") << ", " <<
          "const image_type&" << (columns ? " i" : "") << ", " <<
          "database& db)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << endl;

        inherits (c, init_value_base_inherits_);
        names (c, init_value_member_names_);

        os << "}";
      }

    private:
      bool id_;
      size_t index_;

      grow_base grow_base_;
      traversal::inherits grow_base_inherits_;
      grow_member grow_member_;
      traversal::names grow_member_names_;

      bind_base bind_base_;
      traversal::inherits bind_base_inherits_;
      bind_member bind_member_;
      traversal::names bind_member_names_;
      bind_member bind_id_member_;

      init_image_base init_image_base_;
      traversal::inherits init_image_base_inherits_;
      init_image_member init_image_member_;
      traversal::names init_image_member_names_;

      init_image_member init_id_image_member_;

      init_value_base init_value_base_;
      traversal::inherits init_value_base_inherits_;
      init_value_member init_value_member_;
      traversal::names init_value_member_names_;
    };
  }

  void
  generate_source (context& ctx)
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

    ctx.os << "#include <odb/mysql/mysql.hxx>" << endl
           << "#include <odb/mysql/traits.hxx>" << endl
           << "#include <odb/mysql/database.hxx>" << endl
           << "#include <odb/mysql/transaction.hxx>" << endl
           << "#include <odb/mysql/connection.hxx>" << endl
           << "#include <odb/mysql/statement.hxx>" << endl
           << "#include <odb/mysql/statement-cache.hxx>" << endl
           << "#include <odb/mysql/object-statements.hxx>" << endl
           << "#include <odb/mysql/container-statements.hxx>" << endl
           << "#include <odb/mysql/exceptions.hxx>" << endl;

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/mysql/result.hxx>" << endl;

    ctx.os << endl;

    // Details includes.
    //
    ctx.os << "#include <odb/details/unused.hxx>" << endl;

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/details/shared-ptr.hxx>" << endl;

    ctx.os << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
