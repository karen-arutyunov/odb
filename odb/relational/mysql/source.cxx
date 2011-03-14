// file      : odb/relational/mysql/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/source.hxx>

#include <odb/relational/mysql/common.hxx>
#include <odb/relational/mysql/context.hxx>

using namespace std;

namespace relational
{
  namespace mysql
  {
    namespace source
    {
      namespace relational = relational::source;

      namespace
      {
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
      }

      //
      // bind
      //

      struct bind_member: relational::bind_member, member_base
      {
        bind_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi.t))
            return false;

          ostringstream ostr;
          ostr << "b[n]";
          b = ostr.str ();

          arg = arg_override_.empty () ? string ("i") : arg_override_;

          if (var_override_.empty ())
          {
            os << "// " << mi.m.name () << endl
               << "//" << endl;

            if (inverse (mi.m, key_prefix_))
              os << "if (out)"
                 << "{";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (var_override_.empty ())
          {
            if (semantics::class_* c = comp_value (mi.t))
              os << "n += " << in_column_count (*c) << "UL;";
            else
              os << "n++;";

            if (inverse (mi.m, key_prefix_))
              os << "}";
            else
              os << endl;
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "composite_value_traits< " << mi.fq_type () <<
            " >::bind (b + n, " << arg << "." << mi.var << "value);";
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
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << b << ".buffer_type = " <<
            float_buffer_types[mi.st->type - sql_type::FLOAT] << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_decimal (member_info& mi)
        {
          os << b << ".buffer_type = MYSQL_TYPE_NEWDECIMAL;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".buffer_length = static_cast<unsigned long> (" << endl
             << arg << "." << mi.var << "value.capacity ());"
             << b << ".length = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_date_time (member_info& mi)
        {
          os << b << ".buffer_type = " <<
            date_time_buffer_types[mi.st->type - sql_type::DATE] << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;";

          if (mi.st->type == sql_type::YEAR)
            os << b << ".is_unsigned = 0;";

          os << b << ".is_null = &" << arg << "." << mi.var << "null;";
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
             << arg << "." << mi.var << "value.capacity ());"
             << b << ".length = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_long_string (member_info& mi)
        {
          os << b << ".buffer_type = " <<
            char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".buffer_length = static_cast<unsigned long> (" << endl
             << arg << "." << mi.var << "value.capacity ());"
             << b << ".length = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
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
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_enum (member_info& mi)
        {
          // Represented as a string.
          //
          os << b << ".buffer_type = MYSQL_TYPE_STRING;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".buffer_length = static_cast<unsigned long> (" << endl
             << arg << "." << mi.var << "value.capacity ());"
             << b << ".length = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_set (member_info& mi)
        {
          // Represented as a string.
          //
          os << b << ".buffer_type = MYSQL_TYPE_STRING;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".buffer_length = static_cast<unsigned long> (" << endl
             << arg << "." << mi.var << "value.capacity ());"
             << b << ".length = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

      private:
        string b;
        string arg;
      };
      entry<bind_member> bind_member_;

      //
      // grow
      //

      struct grow_member: relational::grow_member, member_base
      {
        grow_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi.t))
            return false;

          ostringstream ostr;
          ostr << "e[" << index_ << "UL]";
          e = ostr.str ();

          if (var_override_.empty ())
            os << "// " << mi.m.name () << endl
               << "//" << endl;

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (semantics::class_* c = comp_value (mi.t))
            index_ += in_column_count (*c);
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
      };
      entry<grow_member> grow_member_;

      //
      // init image
      //

      struct init_image_member: relational::init_image_member, member_base
      {
        init_image_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x),
              member_image_type_ (base::type_override_,
                                  base::fq_type_override_,
                                  base::key_prefix_),
              member_database_type_id_ (base::type_override_,
                                        base::fq_type_override_,
                                        base::key_prefix_)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          // Ignore containers (they get their own table) and inverse
          // object pointers (they are not present in the 'in' binding).
          //
          if (container (mi.t) || inverse (mi.m, key_prefix_))
            return false;

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
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_type (mi.m, key_prefix_));

            if (semantics::class_* c = object_pointer (mt))
            {
              type = "obj_traits::id_type";
              image_type = member_image_type_.image_type (mi.m);
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              // Handle NULL pointers and extract the id.
              //
              os << "{"
                 << "typedef object_traits< " << c->fq_name () <<
                " > obj_traits;";

              if (weak_pointer (mt))
              {
                os << "typedef pointer_traits< " << mi.fq_type () <<
                  " > wptr_traits;"
                   << "typedef pointer_traits< wptr_traits::" <<
                  "strong_pointer_type > ptr_traits;"
                   << endl
                   << "wptr_traits::strong_pointer_type sp (" <<
                  "wptr_traits::lock (" << member << "));";

                member = "sp";
              }
              else
                os << "typedef pointer_traits< " << mi.fq_type () <<
                  " > ptr_traits;"
                   << endl;

              os << "bool is_null (ptr_traits::null_ptr (" << member << "));"
                 << "if (!is_null)"
                 << "{"
                 << "const " << type << "& id (" << endl;

              if (lazy_pointer (mt))
                os << "ptr_traits::object_id< ptr_traits::element_type  > (" <<
                  member << ")";
              else
                os << "obj_traits::id (ptr_traits::get_ref (" << member << "))";

              os << ");"
                 << endl;

              member = "id";
            }
            else
            {
              type = mi.fq_type ();
              image_type = member_image_type_.image_type (mi.m);
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              os << "{"
                 << "bool is_null;";
            }

            traits = "mysql::value_traits<\n    "
              + type + ",\n    "
              + image_type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (!comp_value (mi.t))
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            if (object_pointer (member_type (mi.m, key_prefix_)))
            {
              os << "}";

              if (!null_pointer (mi.m, key_prefix_))
                os << "else" << endl
                   << "throw null_pointer ();";
            }

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
          os << "std::size_t size (0);"
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
          os << "std::size_t size (0);"
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
          os << "std::size_t size (0);"
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
          os << "std::size_t size (0);"
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
          os << "std::size_t size (0);"
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
          os << "std::size_t size (0);"
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
        string db_type_id;
        string member;
        string image_type;
        string traits;

        member_image_type member_image_type_;
        member_database_type_id member_database_type_id_;
      };
      entry<init_image_member> init_image_member_;

      //
      // init value
      //

      struct init_value_member: relational::init_value_member, member_base
      {
        init_value_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x),
              member_image_type_ (base::type_override_,
                                  base::fq_type_override_,
                                  base::key_prefix_),
              member_database_type_id_ (base::type_override_,
                                        base::fq_type_override_,
                                        base::key_prefix_)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi.t))
            return false;

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
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_type (mi.m, key_prefix_));

            if (semantics::class_* c = object_pointer (mt))
            {
              type = "obj_traits::id_type";
              image_type = member_image_type_.image_type (mi.m);
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              // Handle NULL pointers and extract the id.
              //
              os << "{"
                 << "typedef object_traits< " << c->fq_name () <<
                " > obj_traits;"
                 << "typedef pointer_traits< " << mi.fq_type () <<
                " > ptr_traits;"
                 << endl
                 << "if (i." << mi.var << "null)" << endl;

              if (null_pointer (mi.m, key_prefix_))
                os << member << " = ptr_traits::pointer_type ();";
              else
                os << "throw null_pointer ();";

              os << "else"
                 << "{"
                 << type << " id;";

              member = "id";
            }
            else
            {
              type = mi.fq_type ();
              image_type = member_image_type_.image_type (mi.m);
              db_type_id = member_database_type_id_.database_type_id (mi.m);
            }

            traits = "mysql::value_traits<\n    "
              + type + ",\n    "
              + image_type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (comp_value (mi.t))
            return;

          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          semantics::type& mt (member_type (mi.m, key_prefix_));

          if (object_pointer (mt))
          {
            member = member_override_.empty ()
              ? "o." + mi.m.name ()
              : member_override_;

            if (lazy_pointer (mt))
              os << member << " = ptr_traits::pointer_type (db, id);";
            else
              os << "// If a compiler error points to the line below, then" << endl
                 << "// it most likely means that a pointer used in a member" << endl
                 << "// cannot be initialized from an object pointer." << endl
                 << "//" << endl
                 << member << " = ptr_traits::pointer_type (" << endl
                 << "db.load< ptr_traits::element_type > (id));";

            os << "}"
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
        string db_type_id;
        string image_type;
        string traits;
        string member;

        member_image_type member_image_type_;
        member_database_type_id member_database_type_id_;
      };
      entry<init_value_member> init_value_member_;
    }
  }
}
