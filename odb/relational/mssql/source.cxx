// file      : odb/relational/mssql/source.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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
                string const& table,
                string const& column)
        {
          // Don't add a column for auto id in the INSERT statement.
          //
          if (!(sk_ == statement_insert &&
                key_prefix_.empty () &&
                context::id (m) && auto_(m))) // Only simple id can be auto.
          {
            base::column (m, table, column);
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

      struct bind_member: relational::bind_member_impl<sql_type>,
                          member_base
      {
        bind_member (base const& x)
            : member_base::base (x),      // virtual base
              member_base::base_impl (x), // virtual base
              base_impl (x),
              member_base (x)
        {
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
      };
      entry<bind_member> bind_member_;

      //
      // init image
      //

      struct init_image_member: relational::init_image_member_impl<sql_type>,
                                member_base
      {
        init_image_member (base const& x)
            : member_base::base (x),      // virtual base
              member_base::base_impl (x), // virtual base
              base_impl (x),
              member_base (x)
        {
        }

        virtual void
        set_null (member_info& mi)
        {
          os << "i." << mi.var << "size_ind = SQL_NULL_DATA;";
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
      };
      entry<init_image_member> init_image_member_;

      //
      // init value
      //

      struct init_value_member: relational::init_value_member_impl<sql_type>,
                                member_base
      {
        init_value_member (base const& x)
            : member_base::base (x),      // virtual base
              member_base::base_impl (x), // virtual base
              base_impl (x),
              member_base (x)
        {
        }

        virtual void
        get_null (member_info& mi)
        {
          os << "i." << mi.var << "size_ind == SQL_NULL_DATA";
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
            sql_type const& st (parse_sql_type (i->type, *i->member));

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
          if (options.generate_query () &&
              !(composite (c) || (abstract (c) && !polymorphic (c))))
          {
            type* poly_root (polymorphic (c));
            bool poly_derived (poly_root != 0 && poly_root != &c);

            if (poly_derived)
              os << "{"
                 << "root_traits::image_type& ri (root_image (i));"
                 << endl;

            string i (poly_derived ? "ri" : "i");

            os << "if (" << i << ".change_callback_.callback != 0)" << endl
               << "(" << i << ".change_callback_.callback) (" <<
              i << ".change_callback_.context);";

            if (poly_derived)
              os << "}";
            else
              os << endl;
          }
        }

        virtual void
        init_value_extra ()
        {
          os << "st.stream_result ();";
        }

        virtual void
        persist_statement_extra (type& c,
                                 relational::query_parameters&,
                                 persist_position p)
        {
          semantics::data_member* id (id_member (c));

          type* poly_root (polymorphic (c));
          bool poly_derived (poly_root != 0 && poly_root != &c);

          if (id == 0 || poly_derived || !auto_ (*id))
            return;

          // If we are a derived type in a polymorphic hierarchy, then
          // auto id is handled by the root.
          //
          if (type* root = polymorphic (c))
            if (root != &c)
              return;

          // SQL Server 2005 has a bug that causes it to fail on an
          // INSERT statement with the OUTPUT clause if data for one
          // of the inserted columns is supplied at execution (long
          // data). To work around this problem we use the less
          // efficient batch of INSERT and SELECT statements.
          //
          if (options.mssql_server_version () <= mssql_version (9, 0))
          {
            bool ld (false);

            if (c.count ("mssql-has-long-data"))
              ld = c.get<bool> ("mssql-has-long-data");
            else
            {
              has_long_data t (ld);
              t.traverse (c);
              c.set ("mssql-has-long-data", ld);
            }

            if (ld)
            {
              if (p == persist_after_values)
                os << endl
                   << strlit ("; SELECT SCOPE_IDENTITY()");

              return;
            }
          }

          if (p == persist_after_columns)
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