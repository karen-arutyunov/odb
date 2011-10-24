// file      : odb/relational/oracle/context.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>
#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/relational/oracle/context.hxx>
#include <odb/relational/oracle/common.hxx>

using namespace std;

namespace relational
{
  namespace oracle
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
        {"bool", "NUMBER(1)", 0},

        {"char", "NUMBER(3)", 0},
        {"signed char", "NUMBER(3)", 0},
        {"unsigned char", "NUMBER(3)", 0},

        {"short int", "NUMBER(5)", 0},
        {"short unsigned int", "NUMBER(5)", 0},

        {"int", "NUMBER(10)", 0},
        {"unsigned int", "NUMBER(10)", 0},

        {"long int", "NUMBER(19)", 0},
        {"long unsigned int", "NUMBER(20)", 0},

        {"long long int", "NUMBER(19)", 0},
        {"long long unsigned int", "NUMBER(20)", 0},

        {"float", "BINARY_FLOAT", 0},
        {"double", "BINARY_DOUBLE", 0},

        {"::std::string", "VARCHAR2(4000)", 0},

        {"::size_t", "NUMBER(20)", 0},
        {"::std::size_t", "NUMBER(20)", 0}
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
      need_alias_as = false;
      data_->bind_vector_ = "oracle::bind*";

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
    quote_id_impl (string const& id) const
    {
      string r;
      r.reserve (32);
      r += '"';
      r.append (id, 0, 30);
      r += '"';
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
        r = "NUMBER(10)";

      return r;
    }

    bool context::
    unsigned_integer (semantics::type& t)
    {
      const string& s (t.name ());

      return s == "bool" ||
        s == "unsigned char" ||
        s == "short unsigned int" ||
        s == "unsigned int" ||
        s == "long unsigned int" ||
        s == "long long unsigned int";
    }

    //
    // SQL type parsing.
    //

    sql_type const& context::
    column_sql_type (semantics::data_member& m, string const& kp)
    {
      string key (kp.empty ()
                  ? string ("oracle-column-sql-type")
                  : "oracle-" + kp + "-column-sql-type");

      if (!m.count (key))
      {
        try
        {
          m.set (key, parse_sql_type (column_type (m, kp)));
        }
        catch (invalid_sql_type const& e)
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: " << e.message () << endl;

          throw operation_failed ();
        }
      }

      return m.get<sql_type> (key);
    }

    sql_type context::
    parse_sql_type (string const& sqlt)
    {
      try
      {
        sql_type r;
        sql_lexer l (sqlt);

        // While most type names use single identifier, there are
        // a couple of exceptions to this rule:
        //
        // CHARACTER VARYING          (VARCHAR2)
        // CHAR VARYING               (VARCHAR2)
        // NATIONAL CHARACTER         (NCHAR)
        // NATIONAL CHAR              (NCHAR)
        // NCHAR VARYING              (NVARCHAR2)
        // NATIONAL CHARACTER VARYING (NVARCHAR2)
        // NATIONAL CHAR VARYING      (NVARCHAR2)
        // NCHAR VARYING              (NVARCHAR2)
        // DOUBLE PRECISION           (FLOAT(126))
        //
        enum state
        {
          parse_name,
          parse_range,
          parse_done
        };

        state s (parse_name);
        string prefix;

        for (sql_token t (l.next ());
             s != parse_done && t.type () != sql_token::t_eos;
             t = l.next ())
        {
          sql_token::token_type tt (t.type ());

          switch (s)
          {
          case parse_name:
            {
              if (tt == sql_token::t_identifier)
              {
                string const& id (context::upcase (t.identifier ()));

                //
                // Numeric types.
                //
                if (id == "NUMBER" || id == "DEC" || id == "DECIMAL")
                {
                  // DEC and DECIMAL are equivalent to NUMBER.
                  //
                  r.type = sql_type::NUMBER;
                }
                else if (id == "INT" ||
                         id == "INTEGER" ||
                         id == "SMALLINT")
                {
                  // INT, INTEGER, and SMALLINT map to NUMBER(38). They may not
                  // have range or scale explicitly specified.
                  //
                  r.type = sql_type::NUMBER;
                  r.range = true;
                  r.range_value = 38;
                }
                //
                // Floating point types
                //
                else if (id == "FLOAT")
                {
                  r.type = sql_type::FLOAT;
                  r.range = true;
                  r.range_value = 126;
                }
                else if (id == "DOUBLE")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "PRECISION" && prefix == "DOUBLE")
                {
                  r.type = sql_type::FLOAT;
                  r.range = true;
                  r.range_value = 126;
                }
                else if (id == "REAL")
                {
                  r.type = sql_type::FLOAT;
                  r.range = true;
                  r.range_value = 63;
                }
                else if (id == "BINARY_FLOAT")
                {
                  r.type = sql_type::BINARY_FLOAT;
                }
                else if (id == "BINARY_DOUBLE")
                {
                  r.type = sql_type::BINARY_DOUBLE;
                }
                //
                // Date-time types.
                //
                else if (id == "DATE")
                {
                  r.type = sql_type::DATE;
                }
                else if (id == "TIMESTAMP")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                //
                // Timestamp with time zone.
                //
                else if (id == "WITH" && prefix == "TIMESTAMP")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "TIME" &&
                         (prefix == "TIMESTAMP WITH" ||
                          prefix == "TIMESTAMP WITH LOCAL"))
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "LOCAL" && prefix == "TIMESTAMP WITH")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "ZONE" &&
                         (prefix == "TIMESTAMP WITH LOCAL TIME" ||
                          prefix == "TIMESTAMP WITH TIME"))
                {
                  throw invalid_sql_type (
                    "Oracle timestamps with time zones are not currently "
                    "supported");
                }
                //
                // String and binary types.
                //
                else if (id == "CHAR")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "CHARACTER")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "NCHAR")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "VARCHAR" || id == "VARCHAR2")
                {
                  // VARCHAR is currently mapped to VARCHAR2 in Oracle server.
                  // However, this may change in future versions.
                  //
                  r.type = sql_type::VARCHAR2;
                }
                else if (id == "NVARCHAR2")
                {
                  r.type = sql_type::NVARCHAR2;
                }
                else if (id == "VARYING")
                {
                  // VARYING always appears at the end of an identifier.
                  //
                  if (prefix == "CHAR" || prefix == "CHARACTER")
                    r.type = sql_type::VARCHAR2;
                  else if (prefix == "NCHAR" ||
                           prefix == "NATIONAL CHAR" ||
                           prefix == "NATIONAL CHARACTER")
                    r.type = sql_type::NVARCHAR2;
                }
                else if (id == "NATIONAL")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;

                  continue;
                }
                else if (id == "RAW")
                {
                  r.type = sql_type::RAW;
                }
                //
                // LOB types.
                //
                else if (id == "BLOB")
                {
                  r.type = sql_type::BLOB;
                }
                else if (id == "CLOB")
                {
                  r.type = sql_type::CLOB;
                }
                else if (id == "NCLOB")
                {
                  r.type = sql_type::NCLOB;
                }
                //
                // LONG types.
                //
                else if (id == "LONG")
                {
                  throw invalid_sql_type (
                    "Oracle LONG types are not supported");
                }
                else
                {
                  if (tt == sql_token::t_identifier)
                  {
                    throw invalid_sql_type (
                      "unknown Oracle type '" + t.identifier () + "'");
                  }
                  else
                    throw invalid_sql_type ("expected Oracle type name");
                }
              }

              // Some prefixes can also be type names if not followed
              // by the actual type name.
              //
              if (r.type != sql_type::invalid)
                t = l.next ();
              else if (!prefix.empty ())
              {
                if (prefix == "CHAR" || prefix == "CHARACTER")
                {
                  r.type = sql_type::CHAR;
                  r.range = true;
                  r.range_value = 1;
                  r.byte_semantics = true;
                }
                else if (prefix == "NCHAR" ||
                         prefix == "NATIONAL CHAR" ||
                         prefix == "NATIONAL CHARACTER")
                {
                  r.type = sql_type::NCHAR;
                  r.range = true;
                  r.range_value = 1;
                  r.byte_semantics = false;
                }
                else if (prefix == "TIMESTAMP")
                {
                  r.type = sql_type::TIMESTAMP;
                  r.range = true;
                  r.range_value = 6;
                }

                if (r.type == sql_type::invalid)
                {
                  if (tt == sql_token::t_identifier)
                  {
                    throw invalid_sql_type (
                      "unknown Oracle type '" + t.identifier () + "'");
                  }
                  else
                    throw invalid_sql_type ("expected Oracle type name");
                }
              }

              // Fall through.
              //
              s = parse_range;
            }
          case parse_range:
            {
              if (t.punctuation () == sql_token::p_lparen)
              {
                t = l.next ();

                if (t.type () != sql_token::t_int_lit)
                {
                  throw invalid_sql_type (
                    "integer range expected in Oracle type declaration");
                }

                // Parse the range.
                //
                {
                  unsigned short v;
                  istringstream is (t.literal ());

                  if (!(is >> v && is.eof ()))
                  {
                    throw invalid_sql_type (
                      "invalid range value '" + t.literal () + "' in Oracle "
                      "type declaration");
                  }

                  r.range = true;
                  r.range_value = v;

                  t = l.next ();
                }

                // Parse the scale if present.
                //
                if (t.punctuation () == sql_token::p_comma)
                {
                  t = l.next ();

                  if (t.type () != sql_token::t_int_lit)
                  {
                    throw invalid_sql_type (
                      "integer scale expected in Oracle type declaration");
                  }

                  short v;
                  istringstream is (t.literal ());

                  if (!(is >> v && is.eof ()))
                  {
                    throw invalid_sql_type (
                      "invalid scale value '" + t.literal () + "' in Oracle "
                      "type declaration");
                  }

                  r.scale = true;
                  r.scale_value = v;

                  t = l.next ();
                }
                else if (t.type () == sql_token::t_identifier)
                {
                  const string& id (context::upcase (t.identifier ()));

                  if (id == "CHAR")
                    r.byte_semantics = false;
                  else if (id != "BYTE")
                  {
                    throw invalid_sql_type (
                      "invalid keyword '" + t.literal () + "' in Oracle "
                      "type declaration");
                  }
                }

                if (t.punctuation () != sql_token::p_rparen)
                {
                  throw invalid_sql_type (
                    "expected ')' in Oracle type declaration");
                }
              }

              s = parse_done;
              break;
            }
          case parse_done:
            {
              assert (false);
              break;
            }
          }
        }

        // Some prefixes can also be type names if not followed by the actual
        // type name.
        //
        if (r.type == sql_type::invalid && !prefix.empty ())
        {
          if (prefix == "CHAR" || prefix == "CHARACTER")
          {
            r.type = sql_type::CHAR;
            r.range = true;
            r.range_value = 1;
            r.byte_semantics = true;
          }
          else if (prefix == "NCHAR" ||
                   prefix == "NATIONAL CHAR" ||
                   prefix == "NATIONAL CHARACTER")
          {
            r.type = sql_type::NCHAR;
            r.range = true;
            r.range_value = 1;
            r.byte_semantics = false;
          }
          else if (prefix == "TIMESTAMP")
          {
            r.type = sql_type::TIMESTAMP;
            r.range = true;
            r.range_value = 6;
          }
        }

        if (r.type == sql_type::invalid)
        {
          throw invalid_sql_type (
            "incomplete Oracle type declaration: '" + prefix + "'");
        }

        return r;
      }
      catch (sql_lexer::invalid_input const& e)
      {
        throw invalid_sql_type (
          "invalid Oracle type declaration: " + e.message);
      }
    }
  }
}
