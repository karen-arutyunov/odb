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
      //
      //
      struct object_columns: relational::object_columns, context
      {
        object_columns (base const& x): base (x) {}

        virtual void
        column (semantics::data_member& m,
                string const& key_prefix,
                string const& table,
                string const& column)
        {
          // When we store a ENUM column in the MySQL database, if we bind
          // an integer parameter, then it is treated as an index and if we
          // bind a string, then it is treated as a enumerator. Everything
          // would have worked well if the same logic applied to the select
          // operation. That is, if we bind integer, then the database sends
          // the index and if we bind string then the database sends the
          // enumerator. Unfortunately, MySQL always sends the enumerator
          // and to get the index one has to resort to the enum+0 hack.
          //
          // This causes the following problem: at code generation time we
          // do not yet know which format we want. This is determined at
          // C++ compile time by traits (the reason we don't know this is
          // because we don't want to drag database-specific runtimes,
          // which define the necessary traits, as well as their
          // prerequisites into the ODB compilation process). As a result,
          // we cannot decide at code generation time whether we need the
          // +0 hack or not. One way to overcome this would be to construct
          // the SELECT statements at runtime, something along these lines:
          //
          // "enum" + enum_traits<type>::hack + ","
          //
          // However, this complicates the code generator quite a bit: we
          // either have to move to std::string storage for all the
          // statements and all the databases, which is kind of a waste,
          // or do some deep per-database customizations, which is hairy.
          // So, instead, we are going to use another hack (hey, what the
          // hell, right?) by loading both the index and enumerator
          // combined into a string:
          //
          // CONCAT (enum+0, ' ', enum)
          //
          // For cases where we need the index, everything works since
          // MySQL will convert the leading number and stop at the space.
          // For cases where we need the enumerator, we do a bit of pre-
          // processing (see enum_traits) before handing the value off
          // to value_traits.
          //

          if (sk_ != statement_select ||
              column_sql_type (m, key_prefix).type != sql_type::ENUM)
          {
            base::column (m, key_prefix, table, column);
            return;
          }

          line_ += "CONCAT(";

          if (!table.empty ())
          {
            line_ += table;
            line_ += '.';
          }

          line_ += column;
          line_ += "+0,' ',";

          if (!table.empty ())
          {
            line_ += table;
            line_ += '.';
          }

          line_ += column;

          line_ += ")";
        }
      };
      entry<object_columns> object_columns_;

      struct view_columns: relational::view_columns, context
      {
        view_columns (base const& x): base (x) {}

        virtual void
        column (semantics::data_member& m, string const& column)
        {
          // The same idea as in object_columns.
          //
          if (column_sql_type (m).type != sql_type::ENUM)
          {
            base::column (m, column);
            return;
          }

          line_ += "CONCAT(";
          line_ += column;
          line_ += "+0,' ',";
          line_ += column;
          line_ += ")";
        }
      };
      entry<view_columns> view_columns_;

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

            if (inverse (mi.m, key_prefix_) || version (mi.m))
              os << "if (sk == statement_select)"
                 << "{";
            // If the whole class is readonly, then we will never be
            // called with sk == statement_update.
            //
            else if (!readonly (*context::top_object))
            {
              semantics::class_* c;

              if (id (mi.m) ||
                  readonly (mi.m) ||
                  ((c = composite (mi.t)) && readonly (*c)))
                os << "if (sk != statement_update)"
                   << "{";
            }
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (var_override_.empty ())
          {
            semantics::class_* c;

            if ((c = composite (mi.t)))
            {
              bool ro (readonly (*c));
              column_count_type const& cc (column_count (*c));

              os << "n += " << cc.total << "UL";

              // select = total
              // insert = total - inverse
              // update = total - inverse - readonly
              //
              if (cc.inverse != 0 || (!ro && cc.readonly != 0))
              {
                os << " - (" << endl
                   << "sk == statement_select ? 0 : ";

                if (cc.inverse != 0)
                  os << cc.inverse << "UL";

                if (!ro && cc.readonly != 0)
                {
                  if (cc.inverse != 0)
                    os << " + ";

                  os << "(" << endl
                     << "sk == statement_insert ? 0 : " <<
                    cc.readonly << "UL)";
                }

                os << ")";
              }

              os << ";";
            }
            else
              os << "n++;";

            bool block (false);

            // The same logic as in pre().
            //
            if (inverse (mi.m, key_prefix_) || version (mi.m))
              block = true;
            else if (!readonly (*context::top_object))
            {
              semantics::class_* c;

              if (id (mi.m) ||
                  readonly (mi.m) ||
                  ((c = composite (mi.t)) && readonly (*c)))
                block = true;
            }

            if (block)
              os << "}";
            else
              os << endl;
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "composite_value_traits< " << mi.fq_type () <<
            " >::bind (b + n, " << arg << "." << mi.var << "value, sk);";
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
          // Represented as either integer or string.
          //
          os << "mysql::enum_traits::bind (" << b << "," << endl
             << arg << "." << mi.var << "value," << endl
             << arg << "." << mi.var << "size," << endl
             << "&" << arg << "." << mi.var << "null);";
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
          ostr << "t[" << index_ << "UL]";
          e = ostr.str ();

          if (var_override_.empty ())
            os << "// " << mi.m.name () << endl
               << "//" << endl;

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (semantics::class_* c = composite (mi.t))
            index_ += column_count (*c).total;
          else
            index_++;
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "if (composite_value_traits< " << mi.fq_type () <<
            " >::grow (" << endl
             << "i." << mi.var << "value, t + " << index_ << "UL))"
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
          // Represented as either integer or string (and we don't know
          // at the code generation time which one it is).
          //
          os << "if (" << e << ")" << endl
             << "{"
             << "if (mysql::enum_traits::grow (" <<
            "i." << mi.var << "value, " <<
            "i." << mi.var << "size))" << endl
             << "grew = true;" // String
             << "else" << endl
             << e << " = 0;" // Integer.
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
              member_database_type_id_ (base::type_override_,
                                        base::fq_type_override_,
                                        base::key_prefix_)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          // Ignore containers (they get their own table) and inverse
          // object pointers (they are not present in this binding).
          //
          if (container (mi.t) || inverse (mi.m, key_prefix_))
            return false;

          if (!member_override_.empty ())
            member = member_override_;
          else
          {
            // If we are generating standard init() and this member
            // contains version, ignore it.
            //
            if (version (mi.m))
              return false;

            string const& name (mi.m.name ());
            member = "o." + name;

            os << "// " << name << endl
               << "//" << endl;

            // If the whole class is readonly, then we will never be
            // called with sk == statement_update.
            //
            if (!readonly (*context::top_object))
            {
              semantics::class_* c;

              if (id (mi.m) ||
                  readonly (mi.m) ||
                  ((c = composite (mi.t)) && readonly (*c)))
                os << "if (sk == statement_insert)";
            }
          }

          // If this is a wrapped composite value, then we need to
          // "unwrap" it. For simple values this is taken care of
          // by the value_traits specializations.
          //
          if (mi.wrapper != 0 && composite (mi.t))
          {
            // Here we need the wrapper type, not the wrapped type.
            //
            member = "wrapper_traits< " + mi.fq_type (false) + " >::" +
              "get_ref (" + member + ")";
          }

          if (composite (mi.t))
          {
            os << "{";
            traits = "composite_value_traits< " + mi.fq_type () + " >";
          }
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_utype (mi.m, key_prefix_));

            if (semantics::class_* c = object_pointer (mt))
            {
              type = "obj_traits::id_type";
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
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              os << "{"
                 << "bool is_null;";
            }

            traits = "mysql::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (composite (mi.t))
            os << "}";
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            if (object_pointer (member_utype (mi.m, key_prefix_)))
            {
              os << "}";

              if (!null (mi.m, key_prefix_))
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
          os << "if (" << traits << "::init (" << endl
             << "i." << mi.var << "value," << endl
             << member << "," << endl
             << "sk))" << endl
             << "grew = true;";
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
          // Represented as either integer or string.
          //
          os << "if (mysql::enum_traits::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "is_null," << endl
             << member << "))" << endl
             << "grew = true;";
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
        string traits;

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

            if (mi.cq)
              member = "const_cast< " + mi.fq_type (false) + "& > (" +
                member + ")";

            os << "// " << name << endl
               << "//" << endl;
          }

          // If this is a wrapped composite value, then we need to
          // "unwrap" it. For simple values this is taken care of
          // by the value_traits specializations.
          //
          if (mi.wrapper != 0 && composite (mi.t))
          {
            // Here we need the wrapper type, not the wrapped type.
            //
            member = "wrapper_traits< " + mi.fq_type (false) + " >::" +
              "set_ref (\n" + member + ")";
          }

          if (composite (mi.t))
            traits = "composite_value_traits< " + mi.fq_type () + " >";
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_utype (mi.m, key_prefix_));

            if (semantics::class_* c = object_pointer (mt))
            {
              type = "obj_traits::id_type";
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

              if (null (mi.m, key_prefix_))
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
              db_type_id = member_database_type_id_.database_type_id (mi.m);
            }

            traits = "mysql::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (composite (mi.t))
            return;

          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          semantics::type& mt (member_utype (mi.m, key_prefix_));

          if (object_pointer (mt))
          {
            if (!member_override_.empty ())
              member = member_override_;
            else
            {
              member = "o." + mi.m.name ();

              if (mi.cq)
                member = "const_cast< " + mi.fq_type (false) + "& > (" +
                  member + ")";
            }

            if (lazy_pointer (mt))
              os << member << " = ptr_traits::pointer_type (db, id);";
            else
              os << "// If a compiler error points to the line below, then" << endl
                 << "// it most likely means that a pointer used in a member" << endl
                 << "// cannot be initialized from an object pointer." << endl
                 << "//" << endl
                 << member << " = ptr_traits::pointer_type (" << endl
                 << "db.load< obj_traits::object_type > (id));";

            os << "}"
               << "}";
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << traits << "::init (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "db);"
             << endl;
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
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
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
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
          // Represented as either integer or string.
          //
          os << "mysql::enum_traits::set_value (" << endl
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
        string traits;
        string member;

        member_database_type_id member_database_type_id_;
      };
      entry<init_value_member> init_value_member_;

      struct class_: relational::class_, context
      {
        class_ (base const& x): base (x) {}

        virtual void
        init_auto_id (semantics::data_member&, string const& im)
        {
          os << im << "value = 0;";
        }
      };
      entry<class_> class_entry_;

      struct include: relational::include, context
      {
        include (base const& x): base (x) {}

        virtual void
        extra_post ()
        {
          os << "#include <odb/mysql/enum.hxx>" << endl;
        }
      };
      entry<include> include_;
    }
  }
}
