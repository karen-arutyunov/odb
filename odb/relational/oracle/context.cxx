// file      : odb/relational/oracle/context.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>
#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/relational/oracle/context.hxx>

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

        {"::std::string", "VARCHAR2(512)", 0},

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
      insert_send_auto_id = true;
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
        // INTERVAL YEAR TO MONTH
        // INTERVAL DAY TO SECOND
        //
        enum state
        {
          parse_identifier,
          parse_range,
          parse_done
        };

        state s (parse_identifier);
        string prefix;
        sql_token t (l.next ());

        while (t.type () != sql_token::t_eos)
        {
          sql_token::token_type tt (t.type ());

          switch (s)
          {
          case parse_identifier:
            {
              if (tt == sql_token::t_identifier)
              {
                string const& id (context::upcase (t.identifier ()));

                //
                // Numeric types.
                //
                if ((id == "NUMBER") && prefix.empty ())
                {
                  // If NUMBER has no precision/scale, then it is a floating-
                  // point number. We indicate this by having no range.
                  //
                  r.type = sql_type::NUMBER;
                  s = parse_range;
                }
                else if ((id == "DEC" || id == "DECIMAL" || id == "NUMERIC")
                         && prefix.empty ())
                {
                  // DEC, DECIMAL, and NUMERIC are equivalent to NUMBER in
                  // all ways except that they may not represent a floating
                  // point number. The scale defaults to zero.
                  //
                  r.type = sql_type::NUMBER;
                  s = parse_range;
                }
                else if ((id == "INT" || id == "INTEGER" || id == "SMALLINT")
                         && prefix.empty ())
                {
                  // INT, INTEGER, and SMALLINT map to NUMBER(38). They may not
                  // have range or scale explicitly specified.
                  //
                  r.type = sql_type::NUMBER;
                  r.range = true;
                  r.range_value = 38;

                  s = parse_done;
                }
                //
                // Floating point types
                //
                else if (id == "FLOAT" && prefix.empty ())
                {
                  r.type = sql_type::FLOAT;
                  r.range = true;
                  r.range_value = 126;

                  s = parse_range;
                }
                else if (id == "DOUBLE" && prefix.empty ())
                {
                  prefix = id;
                }
                else if (id == "PRECISION" && prefix == "DOUBLE")
                {
                  r.type = sql_type::FLOAT;
                  r.range = true;
                  r.range_value = 126;

                  s = parse_done;
                }
                else if (id == "REAL" && prefix.empty ())
                {
                  r.type = sql_type::FLOAT;
                  r.range = true;
                  r.range_value = 63;

                  s = parse_done;
                }
                else if (id == "BINARY_FLOAT" && prefix.empty ())
                {
                  r.type = sql_type::BINARY_FLOAT;
                  s = parse_done;
                }
                else if (id == "BINARY_DOUBLE" && prefix.empty ())
                {
                  r.type = sql_type::BINARY_DOUBLE;
                  s = parse_done;
                }
                //
                // Date-time types.
                //
                else if (id == "DATE" && prefix.empty ())
                {
                  r.type = sql_type::DATE;
                  s = parse_done;
                }
                else if (id == "TIMESTAMP" && prefix.empty ())
                {
                  prefix = id;
                }
                else if (id == "INTERVAL" && prefix.empty ())
                {
                  prefix = id;
                }
                else if (id == "YEAR" && prefix == "INTERVAL")
                {
                  prefix += " ";
                  prefix += id;
                  s = parse_range;
                }
                else if (id == "DAY" && prefix == "INTERVAL")
                {
                  prefix += " ";
                  prefix += id;
                  s = parse_range;
                }
                else if (id == "TO" &&
                         (prefix == "INTERVAL YEAR" ||
                          prefix == "INTERVAL DAY"))
                {
                  prefix += " ";
                  prefix += id;
                }
                else if (id == "MONTH" && prefix == "INTERVAL YEAR TO")
                {
                  r.type = sql_type::INTERVAL_YM;
                  s = parse_range;
                }
                else if (id == "SECOND" && prefix == "INTERVAL DAY TO")
                {
                  r.type = sql_type::INTERVAL_DS;
                  s = parse_range;
                }
                //
                // Timestamp with time zone (not supported).
                //
                else if (id == "WITH" && prefix == "TIMESTAMP")
                {
                  prefix += " ";
                  prefix += id;
                }
                else if (id == "TIME" &&
                         (prefix == "TIMESTAMP WITH" ||
                          prefix == "TIMESTAMP WITH LOCAL"))
                {
                  prefix += " ";
                  prefix += id;
                }
                else if (id == "LOCAL" && prefix == "TIMESTAMP WITH")
                {
                  prefix += " ";
                  prefix += id;
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
                }
                else if (id == "CHARACTER")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;
                }
                else if (id == "NCHAR")
                {
                  prefix += prefix.empty () ? "" : " ";
                  prefix += id;
                }
                else if (id == "VARCHAR" || id == "VARCHAR2")
                {
                  // VARCHAR is currently mapped to VARCHAR2 in Oracle server.
                  // However, this may change in future versions.
                  //
                  r.type = sql_type::VARCHAR2;
                  s = parse_range;
                }
                else if (id == "NVARCHAR2")
                {
                  r.type = sql_type::NVARCHAR2;
                  s = parse_range;
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

                  s = parse_range;
                }
                else if (id == "NATIONAL" && prefix.empty ())
                {
                  prefix = id;
                }
                else if (id == "RAW" && prefix.empty ())
                {
                  r.type = sql_type::RAW;
                  s = parse_range;
                }
                //
                // LOB types.
                //
                else if (id == "BLOB" && prefix.empty ())
                {
                  r.type = sql_type::BLOB;
                  s = parse_done;
                }
                else if (id == "CLOB" && prefix.empty ())
                {
                  r.type = sql_type::CLOB;
                  s = parse_done;
                }
                else if (id == "NCLOB" && prefix.empty ())
                {
                  r.type = sql_type::NCLOB;
                  s = parse_done;
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
                  throw invalid_sql_type (
                    "unknown Oracle type '" + t.identifier () + "'");
                }

                t = l.next ();
                continue;
              }
              else if (!prefix.empty ())
              {
                // Some prefixes can also be type names if not followed
                // by the actual type name.
                //

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
                else
                {
                  throw invalid_sql_type (
                    "incomplete Oracle type declaration: '" + prefix + "'");
                }

                // All of the possible types handled in this block can take an
                // optional range specifier. Set the state and fall through to
                // the parse_range handler.
                //
                s = parse_range;
              }
              else
              {
                assert (r.type == sql_type::invalid);

                throw invalid_sql_type (
                  "unexepected '" + t.literal () + "' in Oracle "
                  "type declaration");
              }

              // Fall through.
              //
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
                  // If we are parsing the precision of a TIMESTAMP or INTERVAL
                  // type, there should be no scale present.
                  //
                  if (r.type == sql_type::TIMESTAMP ||
                      string (prefix, 0, 8) == "INTERVAL")
                  {
                    throw invalid_sql_type (
                      "invalid precision in Oracle type declaration");
                  }

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

                  t = l.next ();
                }

                if (t.punctuation () != sql_token::p_rparen)
                {
                  throw invalid_sql_type (
                    "expected ')' in Oracle type declaration");
                }
                else
                  t = l.next ();
              }

              s = r.type == sql_type::invalid ? parse_identifier : parse_done;
              continue;
            }
          case parse_done:
            {
              throw invalid_sql_type (
                "unexepected '" + t.literal () + "' in Oracle "
                "type declaration");

              break;
            }
          }
        }

        // Some prefixes can also be type names if not followed by the actual
        // type name.
        //
        if (r.type == sql_type::invalid)
        {
          if (!prefix.empty ())
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
            else
            {
              throw invalid_sql_type (
                "incomplete Oracle type declaration: '" + prefix + "'");
            }
          }
          else
          {
            throw invalid_sql_type ("invalid Oracle type declaration");
          }
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
