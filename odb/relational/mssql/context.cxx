// file      : odb/relational/mssql/context.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>
#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/relational/mssql/context.hxx>

using namespace std;

namespace relational
{
  namespace mssql
  {
    namespace
    {
      struct type_map_entry
      {
        const char* const cxx_type;
        const char* const db_type;
        const char* const db_id_type;
      };

      type_map_entry type_map[] =
      {
        {"bool", "BIT", 0},

        {"char", "TINYINT", 0},
        {"signed char", "TINYINT", 0},
        {"unsigned char", "TINYINT", 0},

        {"short int", "SMALLINT", 0},
        {"short unsigned int", "SMALLINT", 0},

        {"int", "INT", 0},
        {"unsigned int", "INT", 0},

        {"long int", "BIGINT", 0},
        {"long unsigned int", "BIGINT", 0},

        {"long long int", "BIGINT", 0},
        {"long long unsigned int", "BIGINT", 0},

        {"float", "REAL", 0},
        {"double", "FLOAT", 0},

        {"::std::string", "VARCHAR(512)", "VARCHAR(256)"},
        {"::std::wstring", "NVARCHAR(512)", "NVARCHAR(256)"},

        {"::size_t", "BIGINT", 0},
        {"::std::size_t", "BIGINT", 0},

        // Windows GUID/UUID (typedef struct _GUID {...} GUID, UUID;).
        //
        {"::_GUID", "UNIQUEIDENTIFIER", 0}
      };
    }

    context* context::current_;

    context::
    ~context ()
    {
      if (current_ == this)
        current_ = 0;
    }

    context::
    context (ostream& os,
             semantics::unit& u,
             options_type const& ops,
             sema_rel::model* m)
        : root_context (os, u, ops, data_ptr (new (shared) data (os))),
          base_context (static_cast<data*> (root_context::data_.get ()), m),
          data_ (static_cast<data*> (base_context::data_))
    {
      assert (current_ == 0);
      current_ = this;

      generate_grow = false;
      need_alias_as = true;
      insert_send_auto_id = false;
      delay_freeing_statement_result = true;
      data_->bind_vector_ = "mssql::bind*";

      // Populate the C++ type to DB type map.
      //
      for (size_t i (0); i < sizeof (type_map) / sizeof (type_map_entry); ++i)
      {
        type_map_entry const& e (type_map[i]);

        type_map_type::value_type v (
          e.cxx_type,
          db_type_type (e.db_type, e.db_id_type ? e.db_id_type : e.db_type));

        data_->type_map_.insert (v);
      }
    }

    context::
    context ()
        : data_ (current ().data_)
    {
    }

    string context::
    quote_id_impl (qname const& id) const
    {
      string r;

      bool f (true);
      for (qname::iterator i (id.begin ()); i < id.end (); ++i)
      {
        if (i->empty ())
          continue;

        if (f)
          f = false;
        else
          r += '.';

        r += '[';
        r.append (*i, 0, 128); // Max identifier length is 128.
        r += ']';
      }

      return r;
    }

    string context::
    database_type_impl (semantics::type& t, semantics::names* hint, bool id)
    {
      string r (base_context::database_type_impl (t, hint, id));

      if (!r.empty ())
        return r;

      using semantics::enum_;

      if (t.is_a<semantics::enum_> ())
        r = "INT";

      return r;
    }

    //
    // SQL type parsing.
    //

    namespace
    {
      struct sql_parser
      {
        typedef context::invalid_sql_type invalid_sql_type;

        sql_parser (std::string const& sql)
            : l_ (sql)
        {
        }

        sql_type
        parse ()
        {
          r_ = sql_type ();

          try
          {
            parse_name ();
          }
          catch (sql_lexer::invalid_input const& e)
          {
            throw invalid_sql_type ("invalid SQL Server type declaration: " +
                                    e.message);
          }

          return r_;
        }

        void
        parse_name ()
        {
          sql_token t (l_.next ());

          if (t.type () != sql_token::t_identifier)
          {
            throw invalid_sql_type ("expected SQL Server type name "
                                    "instead of '" + t.string () + "'");
          }

          string id (upcase (t.identifier ()));

          if (id == "BIT")
          {
            r_.type = sql_type::BIT;
          }
          else if (id == "TINYINT")
          {
            r_.type = sql_type::TINYINT;
          }
          else if (id == "SMALLINT")
          {
            r_.type = sql_type::SMALLINT;
          }
          else if (id == "INT" ||
                   id == "INTEGER")
          {
            r_.type = sql_type::INT;
          }
          else if (id == "BIGINT")
          {
            r_.type = sql_type::BIGINT;
          }
          else if (id == "DECIMAL" ||
                   id == "NUMERIC" ||
                   id == "DEC")
          {
            r_.type = sql_type::DECIMAL;

            r_.has_prec = true;
            r_.prec = 18;

            r_.has_scale = true;
            r_.scale = 0;

            parse_precision (l_.next ());
          }
          else if (id == "SMALLMONEY")
          {
            r_.type = sql_type::SMALLMONEY;
          }
          else if (id == "MONEY")
          {
            r_.type = sql_type::MONEY;
          }
          else if (id == "REAL")
          {
            r_.type = sql_type::FLOAT;

            r_.has_prec = true;
            r_.prec = 24;
          }
          else if (id == "FLOAT")
          {
            r_.type = sql_type::FLOAT;

            r_.has_prec = true;
            r_.prec = 53;

            parse_precision (l_.next ());
          }
          else if (id == "DOUBLE")
          {
            t = l_.next ();

            if (t.type () != sql_token::t_identifier ||
                upcase (t.identifier ()) != "PRECISION")
            {
              throw invalid_sql_type ("expected 'PRECISION' instead of '"
                                      + t.string () + "'");
            }

            r_.type = sql_type::FLOAT;

            r_.has_prec = true;
            r_.prec = 53;

            // It appears that DOUBLE PRECISION can be follows by the
            // precision specification.
            //
            parse_precision (l_.next ());
          }
          else if (id == "CHAR" ||
                   id == "CHARACTER")
          {
            parse_char_trailer (false);
          }
          else if (id == "VARCHAR")
          {
            r_.type = sql_type::VARCHAR;

            r_.has_prec = true;
            r_.prec = 1;

            parse_precision (l_.next ());
          }
          else if (id == "TEXT")
          {
            r_.type = sql_type::TEXT;
          }
          else if (id == "NCHAR")
          {
            r_.type = sql_type::NCHAR;

            r_.has_prec = true;
            r_.prec = 1;

            parse_precision (l_.next ());
          }
          else if (id == "NVARCHAR")
          {
            r_.type = sql_type::NVARCHAR;

            r_.has_prec = true;
            r_.prec = 1;

            parse_precision (l_.next ());
          }
          else if (id == "NTEXT")
          {
            r_.type = sql_type::NTEXT;
          }
          else if (id == "NATIONAL")
          {
            t = l_.next ();

            if (t.type () == sql_token::t_identifier)
              id = upcase (t.identifier ());

            if (id == "TEXT")
            {
              r_.type = sql_type::NTEXT;
            }
            else if (id == "CHAR" ||
                     id == "CHARACTER")
            {
              parse_char_trailer (true);
            }
            else
            {
              throw invalid_sql_type (
                "expected 'CHAR', 'CHARACTER', or 'TEXT' instead of '"
                + t.string () + "'");
            }
          }
          else if (id == "BINARY")
          {
            // Can be just BINARY or BINARY VARYING.
            //
            t = l_.next ();

            if (t.type () == sql_token::t_identifier)
              id = upcase (t.identifier ());

            if (id == "VARYING")
            {
              r_.type = sql_type::VARBINARY;
              t = l_.next ();
            }
            else
              r_.type = sql_type::BINARY;

            r_.has_prec = true;
            r_.prec = 1;

            parse_precision (t);
          }
          else if (id == "VARBINARY")
          {
            r_.type = sql_type::VARBINARY;

            r_.has_prec = true;
            r_.prec = 1;

            parse_precision (l_.next ());
          }
          else if (id == "IMAGE")
          {
            r_.type = sql_type::IMAGE;
          }
          else if (id == "DATE")
          {
            r_.type = sql_type::DATE;
          }
          else if (id == "TIME")
          {
            r_.type = sql_type::TIME;

            r_.has_scale = true;
            r_.scale = 7;

            parse_precision (l_.next ());
          }
          else if (id == "DATETIME")
          {
            r_.type = sql_type::DATETIME;
          }
          else if (id == "DATETIME2")
          {
            r_.type = sql_type::DATETIME2;

            r_.has_scale = true;
            r_.scale = 7;

            parse_precision (l_.next ());
          }
          else if (id == "SMALLDATETIME")
          {
            r_.type = sql_type::SMALLDATETIME;
          }
          else if (id == "DATETIMEOFFSET")
          {
            r_.type = sql_type::DATETIMEOFFSET;

            r_.has_scale = true;
            r_.scale = 7;

            parse_precision (l_.next ());
          }
          else if (id == "UNIQUEIDENTIFIER")
          {
            r_.type = sql_type::UNIQUEIDENTIFIER;
          }
          else if (id == "ROWVERSION" ||
                   id == "TIMESTAMP")
          {
            r_.type = sql_type::ROWVERSION;
          }
          else
          {
            throw invalid_sql_type ("unexpected SQL Server type name '" +
                                    t.identifier () + "'");
          }
        }

        void
        parse_precision (sql_token t)
        {
          if (t.punctuation () == sql_token::p_lparen)
          {
            // Parse the precision.
            //
            t = l_.next ();

            if (t.type () == sql_token::t_identifier &&
                upcase (t.identifier ()) == "MAX")
            {
              r_.prec = 0;
              r_.has_prec = true;
            }
            else if (t.type () == sql_token::t_int_lit)
            {
              unsigned short v;
              istringstream is (t.literal ());

              if (!(is >> v && is.eof ()))
              {
                throw invalid_sql_type (
                  "invalid precision value '" + t.literal () + "' in SQL "
                  "Server type declaration");
              }

              switch (r_.type)
              {
              case sql_type::TIME:
              case sql_type::DATETIME2:
              case sql_type::DATETIMEOFFSET:
                {
                  r_.scale = v;
                  r_.has_scale = true;
                  break;
                }
              default:
                {
                  r_.prec = v;
                  r_.has_prec = true;
                  break;
                }
              }
            }
            else
            {
              throw invalid_sql_type (
                "integer precision expected in SQL Server type declaration");
            }

            // Parse the scale if present.
            //
            t = l_.next ();

            if (t.punctuation () == sql_token::p_comma)
            {
              // Scale can only be specified for the DECIMAL type.
              //
              if (r_.type != sql_type::DECIMAL)
              {
                throw invalid_sql_type (
                  "unexpected scale in SQL Server type declaration");
              }

              t = l_.next ();

              if (t.type () != sql_token::t_int_lit)
              {
                throw invalid_sql_type (
                  "integer scale expected in SQL Server type declaration");
              }

              istringstream is (t.literal ());

              if (!(is >> r_.scale && is.eof ()))
              {
                throw invalid_sql_type (
                  "invalid scale value '" + t.literal () + "' in SQL Server "
                  "type declaration");
              }

              r_.has_scale = true;
              t = l_.next ();
            }

            if (t.punctuation () != sql_token::p_rparen)
            {
              throw invalid_sql_type (
                "expected ')' in SQL Server type declaration");
            }
          }
        }

        void
        parse_char_trailer (bool nat)
        {
          sql_token t (l_.next ());

          string id;

          if (t.type () == sql_token::t_identifier)
            id = upcase (t.identifier ());

          if (id == "VARYING")
          {
            r_.type = nat ? sql_type::NVARCHAR : sql_type::VARCHAR;
            t = l_.next ();
          }
          else
            r_.type = nat ? sql_type::NCHAR : sql_type::CHAR;

          r_.has_prec = true;
          r_.prec = 1;

          parse_precision (t);
        }

      private:
        string
        upcase (string const& s)
        {
          return context::upcase (s);
        }

      private:
        sql_lexer l_;
        sql_type r_;
      };
    }

    sql_type const& context::
    parse_sql_type (string const& t, semantics::data_member& m)
    {
      // If this proves to be too expensive, we can maintain a
      // cache of parsed types.
      //
      data::sql_type_cache::iterator i (data_->sql_type_cache_.find (t));

      if (i != data_->sql_type_cache_.end ())
        return i->second;
      else
      {
        try
        {
          return (data_->sql_type_cache_[t] = parse_sql_type (t));
        }
        catch (invalid_sql_type const& e)
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: " << e.message () << endl;

          throw operation_failed ();
        }
      }
    }

    sql_type context::
    parse_sql_type (string const& t)
    {
      sql_parser p (t);
      return p.parse ();
    }
  }
}
