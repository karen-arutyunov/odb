// file      : odb/relational/mssql/source.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <sstream>

#include <odb/relational/source.hxx>

#include <odb/relational/mssql/common.hxx>
#include <odb/relational/mssql/context.hxx>

using namespace std;

namespace relational
{
  namespace mssql
  {
    namespace source
    {
      namespace relational = relational::source;

      //
      //
      struct query_parameters: relational::query_parameters
      {
        query_parameters (base const& x): base (x) {}

        virtual string
        auto_id ()
        {
          return "";
        }
      };
      entry<query_parameters> query_parameters_;

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
          // Don't add a column for auto id in the INSERT statement.
          //
          if (!(sk_ == statement_insert &&
                key_prefix.empty () &&
                id (m) && auto_(m)))
          {
            base::column (m, key_prefix, table, column);
          }
        }
      };
      entry<object_columns> object_columns_;

      //
      // bind
      //

      static const char* integer_buffer_types[] =
      {
        "mssql::bind::bit",
        "mssql::bind::tinyint",
        "mssql::bind::smallint",
        "mssql::bind::int_",
        "mssql::bind::bigint"
      };

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
          if (container (mi))
            return false;

          ostringstream ostr;
          ostr << "b[n]";
          b = ostr.str ();

          arg = arg_override_.empty () ? string ("i") : arg_override_;

          if (var_override_.empty ())
          {
            os << "// " << mi.m.name () << endl
               << "//" << endl;

            if (id (mi.m) && auto_ (mi.m))
              // For SQL Server we don't send auto id in INSERT.
              //
              os << "if (sk != statement_insert && sk != statement_update)"
                 << "{";
            else if (inverse (mi.m, key_prefix_) || version (mi.m))
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
            if (id (mi.m) && auto_ (mi.m))
              block = true;
            else if (inverse (mi.m, key_prefix_) || version (mi.m))
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
          os << b << ".type = " <<
            integer_buffer_types[mi.st->type - sql_type::BIT] << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;";
        }

        virtual void
        traverse_decimal (member_info& mi)
        {
          os << b << ".type = mssql::bind::decimal;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode precision (p) and scale (s) as (p * 100 + s).
            //
             << b << ".capacity = " << mi.st->prec * 100 + mi.st->scale << ";";
        }

        virtual void
        traverse_smallmoney (member_info& mi)
        {
          os << b << ".type = mssql::bind::smallmoney;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;";
        }

        virtual void
        traverse_money (member_info& mi)
        {
          os << b << ".type = mssql::bind::money;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;";
        }

        virtual void
        traverse_float4 (member_info& mi)
        {
          os << b << ".type = mssql::bind::float4;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
             << b << ".capacity = " << mi.st->prec << ";";
        }

        virtual void
        traverse_float8 (member_info& mi)
        {
          os << b << ".type = mssql::bind::float8;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
             << b << ".capacity = " << mi.st->prec << ";";
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << b << ".type = mssql::bind::string;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
             << b << ".capacity = static_cast<SQLLEN> (sizeof (" <<
            arg << "." << mi.var << "value));";
        }

        virtual void
        traverse_long_string (member_info& mi)
        {
          os << b << ".type = mssql::bind::long_string;"
             << b << ".buffer = &" << arg << "." << mi.var << "callback;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode the column size with 0 indicating unlimited.
            //
             << b << ".capacity = " << mi.st->prec << ";";
        }

        virtual void
        traverse_nstring (member_info& mi)
        {
          os << b << ".type = mssql::bind::nstring;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
             << b << ".capacity = static_cast<SQLLEN> (sizeof (" <<
            arg << "." << mi.var << "value));";
        }

        virtual void
        traverse_long_nstring (member_info& mi)
        {
          os << b << ".type = mssql::bind::long_nstring;"
             << b << ".buffer = &" << arg << "." << mi.var << "callback;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode the column size (in bytes) with 0 indicating unlimited.
            //
             << b << ".capacity = " << mi.st->prec * 2 << ";";
        }

        virtual void
        traverse_binary (member_info& mi)
        {
          os << b << ".type = mssql::bind::binary;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
             << b << ".capacity = static_cast<SQLLEN> (sizeof (" <<
            arg << "." << mi.var << "value));";
        }

        virtual void
        traverse_long_binary (member_info& mi)
        {
          os << b << ".type = mssql::bind::long_binary;"
             << b << ".buffer = &" << arg << "." << mi.var << "callback;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode the column size with 0 indicating unlimited.
            //
             << b << ".capacity = " << mi.st->prec << ";";
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << b << ".type = mssql::bind::date;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;";
        }

        virtual void
        traverse_time (member_info& mi)
        {
          os << b << ".type = mssql::bind::time;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode fractional seconds (scale).
            //
             << b << ".capacity = " << mi.st->scale << ";";
        }

        virtual void
        traverse_datetime (member_info& mi)
        {
          unsigned short scale (0);

          switch (mi.st->type)
          {
          case sql_type::DATETIME:
            {
              // Looks like it is 3 (rounded to 0.000, 0.003, or 0.007).
              //
              scale = 3;
              break;
            }
          case sql_type::DATETIME2:
            {
              scale = mi.st->scale;
              break;
            }
          case sql_type::SMALLDATETIME:
            {
              // No seconds in SMALLDATATIME. Encode it a special precision
              // value (8).
              //
              scale = 8;
              break;
            }
          default:
            {
              assert (false);
              break;
            }
          }

          os << b << ".type = mssql::bind::datetime;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode fractional seconds (scale).
            //
             << b << ".capacity = " << scale << ";";
        }

        virtual void
        traverse_datetimeoffset (member_info& mi)
        {
          os << b << ".type = mssql::bind::datetimeoffset;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;"
            // Encode fractional seconds (scale).
            //
             << b << ".capacity = " << mi.st->scale << ";";
        }

        virtual void
        traverse_uniqueidentifier (member_info& mi)
        {
          os << b << ".type = mssql::bind::uniqueidentifier;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;";
        }

        virtual void
        traverse_rowversion (member_info& mi)
        {
          os << b << ".type = mssql::bind::rowversion;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".size_ind = &" << arg << "." << mi.var << "size_ind;";
        }

      private:
        string b;
        string arg;
      };
      entry<bind_member> bind_member_;

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
          if (container (mi) || inverse (mi.m, key_prefix_))
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

            // For SQL Server we don't send auto id in INSERT statement
            // (nor in UPDATE, as for other databases). So ignore it
            // altogether.
            //
            if (id (mi.m) && auto_ (mi.m))
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
                 << "typedef object_traits< " << class_fq_name (*c) <<
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

            traits = "mssql::value_traits<\n    "
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
              os << "}"
                 << "else" << endl;

              if (!null (mi.m, key_prefix_))
                os << "throw null_pointer ();";
              else
                os << "i." << mi.var << "size_ind = SQL_NULL_DATA;";
            }

            os << "}";
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << traits << "::init (" << endl
             << "i." << mi.var << "value," << endl
             << member << "," << endl
             << "sk);";
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_decimal (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_smallmoney (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 4;";
        }

        virtual void
        traverse_money (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 8;";
        }

        virtual void
        traverse_float4 (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_float8 (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
            // Don't mention the extra character for the null-terminator.
             << "sizeof (i." << mi.var << "value) - 1," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size_ind =" << endl
             << "  is_null ? SQL_NULL_DATA : static_cast<SQLLEN> (size);";
        }

        virtual void
        traverse_long_string (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "callback.callback.param," << endl
             << "i." << mi.var << "callback.context.param," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size_ind = is_null ? " <<
            "SQL_NULL_DATA : SQL_DATA_AT_EXEC;";
        }

        virtual void
        traverse_nstring (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
            // Don't mention the extra character for the null-terminator.
             << "sizeof (i." << mi.var << "value) / 2 - 1," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size_ind =" << endl
             << "  is_null ? SQL_NULL_DATA : static_cast<SQLLEN> (size * 2);";
        }

        virtual void
        traverse_long_nstring (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "callback.callback.param," << endl
             << "i." << mi.var << "callback.context.param," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size_ind = is_null ? " <<
            "SQL_NULL_DATA : SQL_DATA_AT_EXEC;";
        }

        virtual void
        traverse_binary (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "sizeof (i." << mi.var << "value)," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size_ind =" << endl
             << "  is_null ? SQL_NULL_DATA : static_cast<SQLLEN> (size);";
        }

        virtual void
        traverse_long_binary (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "callback.callback.param," << endl
             << "i." << mi.var << "callback.context.param," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size_ind = is_null ? " <<
            "SQL_NULL_DATA : SQL_DATA_AT_EXEC;";
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_time (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, " << mi.st->scale << ", " <<
            "is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null" << endl
             << "  ? SQL_NULL_DATA" << endl
             << "  : static_cast<SQLLEN> (sizeof (i." << mi.var << "value));";
        }

        virtual void
        traverse_datetime (member_info& mi)
        {
          // The same code as in bind.
          //
          unsigned short scale (0);

          switch (mi.st->type)
          {
          case sql_type::DATETIME:
            {
              scale = 3;
              break;
            }
          case sql_type::DATETIME2:
            {
              scale = mi.st->scale;
              break;
            }
          case sql_type::SMALLDATETIME:
            {
              scale = 8;
              break;
            }
          default:
            {
              assert (false);
              break;
            }
          }

          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, " << scale << ", " <<
            "is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_datetimeoffset (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, " << mi.st->scale << ", " <<
            "is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null" << endl
             << "  ? SQL_NULL_DATA" << endl
             << "  : static_cast<SQLLEN> (sizeof (i." << mi.var << "value));";
        }

        virtual void
        traverse_uniqueidentifier (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 0;";
        }

        virtual void
        traverse_rowversion (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "size_ind = is_null ? SQL_NULL_DATA : 8;";
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
          if (container (mi))
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
                 << "typedef object_traits< " << class_fq_name (*c) <<
                " > obj_traits;"
                 << "typedef pointer_traits< " << mi.fq_type () <<
                " > ptr_traits;"
                 << endl
                 << "if (i." << mi.var << "size_ind == SQL_NULL_DATA)" << endl;

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

            traits = "mssql::value_traits<\n    "
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
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_decimal (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_smallmoney (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_money (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_float4 (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_float8 (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "static_cast<std::size_t> (i." << mi.var << "size_ind)," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_long_string (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "callback.callback.result," << endl
             << "i." << mi.var << "callback.context.result);"
             << endl;
        }

        virtual void
        traverse_nstring (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "static_cast<std::size_t> (" <<
            "i." << mi.var << "size_ind / 2)," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_long_nstring (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "callback.callback.result," << endl
             << "i." << mi.var << "callback.context.result);"
             << endl;
        }

        virtual void
        traverse_binary (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "static_cast<std::size_t> (i." << mi.var << "size_ind)," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_long_binary (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "callback.callback.result," << endl
             << "i." << mi.var << "callback.context.result);"
             << endl;
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_time (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_datetime (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_datetimeoffset (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_uniqueidentifier (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
             << endl;
        }

        virtual void
        traverse_rowversion (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size_ind == SQL_NULL_DATA);"
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

      struct statement_columns_common: context
      {
        void
        process (relational::statement_columns& cols, statement_kind sk)
        {
          using relational::statement_columns;

          // Long data columns must come last in the SELECT statement.
          //
          if (sk != statement_select)
            return;

          // Go over the columns list while keeping track of how many
          // columns we have examined. If the current column is long data,
          // then move it to the back. Stop once we have examined all the
          // columns.
          //
          size_t n (cols.size ());
          for (statement_columns::iterator i (cols.begin ()); n != 0; --n)
          {
            sql_type const& st (column_sql_type (*i->member, i->key_prefix));

            bool l (false);

            // The same "short/long data" tests as in common.cxx.
            //
            switch (st.type)
            {
            case sql_type::CHAR:
            case sql_type::VARCHAR:
            case sql_type::BINARY:
            case sql_type::VARBINARY:
              {
                // Zero precision means max in VARCHAR(max).
                //
                if (st.prec == 0 || st.prec > options.mssql_short_limit ())
                  l = true;

                break;
              }
            case sql_type::NCHAR:
            case sql_type::NVARCHAR:
              {
                // Zero precision means max in NVARCHAR(max). Note that
                // the precision is in 2-byte UCS-2 characters, not bytes.
                //
                if (st.prec == 0 || st.prec * 2 > options.mssql_short_limit ())
                  l = true;

                break;
              }
            case sql_type::TEXT:
            case sql_type::NTEXT:
            case sql_type::IMAGE:
              {
                l = true;
                break;
              }
            default:
              break;
            }

            if (l)
            {
              cols.push_back (*i);
              i = cols.erase (i);
            }
            else
              ++i;
          }
        }
      };

      struct container_traits: relational::container_traits,
                               statement_columns_common
      {
        container_traits (base const& x): base (x) {}

        virtual void
        cache_result (string const&)
        {
          // Caching is not necessary since with MARS enabled SQL Server
          // can execute several interleaving statements.
          //
        }

        virtual void
        init_value_extra ()
        {
          os << "sts.select_all_statement ().stream_result ();"
             << endl;
        }

        virtual void
        process_statement_columns (relational::statement_columns& cols,
                                   statement_kind sk)
        {
          statement_columns_common::process (cols, sk);
        }
      };
      entry<container_traits> container_traits_;

      struct class_: relational::class_, statement_columns_common
      {
        class_ (base const& x): base (x) {}

        virtual void
        init_image_pre (type& c)
        {
          if (options.generate_query () && !(composite (c) || abstract (c)))
            os << "if (i.change_callback_.callback != 0)" << endl
               << "(i.change_callback_.callback) (i.change_callback_.context);"
               << endl;
        }

        virtual void
        init_value_extra ()
        {
          os << "sts.find_statement ().stream_result ();";
        }

        virtual void
        free_statement_result_immediate ()
        {
          // Only free the result if there are no rows. Otherwise we
          // need to keep the result alive until after we are done
          // streaming long data.
          //
          os << "if (r == select_statement::no_data)" << endl
             << "st.free_result ();"
             << endl;
        }

        virtual void
        free_statement_result_delayed ()
        {
          os << "sts.find_statement ().free_result ();";
        }

        virtual void
        persist_statement_extra (type& c,
                                 relational::query_parameters&,
                                 persist_position p)
        {
          if (p != persist_after_columns)
            return;

          semantics::data_member* id (id_member (c));

          if (id != 0 && id->count ("auto"))
            os << strlit (" OUTPUT INSERTED." + column_qname (*id)) << endl;
        }

        virtual void
        process_statement_columns (relational::statement_columns& cols,
                                   statement_kind sk)
        {
          statement_columns_common::process (cols, sk);
        }
      };
      entry<class_> class_entry_;
    }
  }
}
