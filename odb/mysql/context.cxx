// file      : odb/mysql/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/mysql/context.hxx>

using namespace std;

namespace mysql
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
      {"bool", "TINYINT(1)", 0},

      {"char", "TINYINT", 0},
      {"signed char", "TINYINT", 0},
      {"unsigned char", "TINYINT UNSIGNED", 0},

      {"short int", "SMALLINT", 0},
      {"short unsigned int", "SMALLINT UNSIGNED", 0},

      {"int", "INT", 0},
      {"unsigned int", "INT UNSIGNED", 0},

      {"long int", "BIGINT", 0},
      {"long unsigned int", "BIGINT UNSIGNED", 0},

      {"long long int", "BIGINT", 0},
      {"long long unsigned int", "BIGINT UNSIGNED", 0},

      {"float", "FLOAT", 0},
      {"double", "DOUBLE", 0},

      {"::std::string", "TEXT", "VARCHAR(255)"}
    };
  }

  context::
  context (ostream& os, semantics::unit& u, options_type const& ops)
      : base_context (os, u, ops, data_ptr (new (shared) data)),
        data_ (static_cast<data*> (base_context::data_.get ()))
  {
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
  context (context& c)
      : base_context (c),
        data_ (c.data_)
  {
  }

  static sql_type
  parse_sql_type (semantics::data_member& m, std::string const& sql);

  sql_type const& context::
  db_type (semantics::data_member& m)
  {
    if (!m.count ("db-type"))
      m.set ("db-type", parse_sql_type (m, column_type (m)));

    return m.get<sql_type> ("db-type");
  }

  static sql_type
  parse_sql_type (semantics::data_member& m, string const& sql)
  {
    try
    {
      sql_type r;
      sql_lexer l (sql);

      // While most type names use single identifier, there are
      // a couple of exceptions to this rule:
      //
      // NATIONAL CHAR|VARCHAR
      // CHAR BYTE             (BINARY)
      // CHARACTER VARYING     (VARCHAR)
      // LONG VARBINARY        (MEDIUMBLOB)
      // LONG VARCHAR          (MEDIUMTEXT)
      //
      //
      enum state
      {
        parse_prefix,
        parse_name,
        parse_bounds,
        parse_sign
      };

      state s (parse_prefix);

      for (sql_token t (l.next ());
           t.type () != sql_token::t_eos; t = l.next ())
      {
        sql_token::token_type tt (t.type ());

        switch (s)
        {
        case parse_prefix:
          {
            if (tt == sql_token::t_identifier)
            {
              string const& id (t.identifier ());

              if (id == "NATIONAL" ||
                  id == "CHAR" ||
                  id == "CHARACTER" ||
                  id == "LONG")
              {
                r.type = id;
                continue;
              }
            }

            // Fall through.
            //
            s = parse_prefix;
          }
        case parse_name:
          {
            if (tt == sql_token::t_identifier)
            {
              bool match (true);
              string const& id (t.identifier ());

              // Numeric types.
              //
              if (id == "BIT")
              {
                r.type = "BIT";
              }
              else if (id == "TINYINT" || id == "INT1")
              {
                r.type = "TINYINT";
              }
              else if (id == "BOOL" || id == "BOOLEAN")
              {
                r.type = "TINYINT";
                r.bounds = true;
                r.bounds_value = 1;
              }
              else if (id == "SMALLINT" || id == "INT2")
              {
                r.type = "SMALLINT";
              }
              else if (id == "MEDIUMINT" || id == "INT3" || id == "MIDDLEINT")
              {
                r.type = "MEDIUMINT";
              }
              else if (id == "INT" || id == "INTEGER" || id == "INT4")
              {
                r.type = "INT";
              }
              else if (id == "BIGINT" || id == "INT8")
              {
                r.type = "BIGINT";
              }
              else if (id == "SERIAL")
              {
                r.type = "BIGINT";
                r.unsign = true;
              }
              else if (id == "FLOAT" || id == "FLOAT4")
              {
                r.type = "FLOAT";
              }
              else if (id == "DOUBLE" || id == "FLOAT8")
              {
                r.type = "DOUBLE";
              }
              else if (id == "DECIMAL" ||
                       id == "DEC" ||
                       id == "NUMERIC" ||
                       id == "FIXED")
              {
                r.type = "DECIMAL";
              }
              //
              // Date-time types.
              //
              else if (id == "DATE")
              {
                r.type = "DATE";
              }
              else if (id == "TIME")
              {
                r.type = "TIME";
              }
              else if (id == "DATETIME")
              {
                r.type = "DATETIME";
              }
              else if (id == "TIMESTAMP")
              {
                r.type = "TIMESTAMP";
              }
              else if (id == "YEAR")
              {
                r.type = "YEAR";
              }
              //
              // String and binary types.
              //
              else if (id == "NCHAR")
              {
                r.type = "CHAR";
              }
              else if (id == "VARCHAR")
              {
                r.type = r.type == "LONG" ? "MEDIUMTEXT" : "VARCHAR";
              }
              else if (id == "NVARCHAR")
              {
                r.type = "VARCHAR";
              }
              else if (id == "VARYING" && r.type == "CHARACTER")
              {
                r.type = "VARCHAR";
              }
              else if (id == "BINARY")
              {
                r.type = "BINARY";
              }
              else if (id == "BYTE" && r.type == "CHAR")
              {
                r.type = "BINARY";
              }
              else if (id == "VARBINARY")
              {
                r.type = r.type == "LONG" ? "MEDIUMBLOB" : "VARBINARY";
              }
              else if (id == "TINYBLOB")
              {
                r.type = "TINYBLOB";
              }
              else if (id == "TINYTEXT")
              {
                r.type = "TINYTEXT";
              }
              else if (id == "BLOB")
              {
                r.type = "BLOB";
              }
              else if (id == "TEXT")
              {
                r.type = "TEXT";
              }
              else if (id == "MEDIUMBLOB")
              {
                r.type = "MEDIUMBLOB";
              }
              else if (id == "MEDIUMTEXT")
              {
                r.type = "MEDIUMTEXT";
              }
              else if (id == "LONGBLOB")
              {
                r.type = "LONGBLOB";
              }
              else if (id == "LONGTEXT")
              {
                r.type = "LONGTEXT";
              }
              else if (id == "ENUM")
              {
                r.type = "ENUM";
              }
              else if (id == "SET")
              {
                r.type = "SET";
              }
              else
                match = false;

              if (match)
              {
                s = parse_bounds;
                continue;
              }
            }

            bool match (false);

            // Some prefixes can also be type names if not followed
            // by the actual type name.
            //
            if (!r.type.empty ())
            {
              if (r.type == "CHAR")
                match = true;
              else if (r.type == "CHARACTER")
              {
                r.type = "CHAR";
                match = true;
              }
              else if (r.type == "LONG")
              {
                r.type = "MEDIUMTEXT";
                match = true;
              }
            }

            if (!match)
            {
              cerr << m.file () << ":" << m.line () << ":" << m.column ();

              if (tt == sql_token::t_identifier)
                cerr << " error: unknown MySQL type '" <<
                  t.identifier () << "'" << endl;
              else
                cerr << " error: expected MySQL type name" << endl;

              throw generation_failed ();
            }

            // Fall through.
            //
            s = parse_bounds;
          }
        case parse_bounds:
          {
            if (t.punctuation () == sql_token::p_lparen)
            {
              t = l.next ();

              if (t.type () != sql_token::t_int_lit)
              {
                cerr << m.file () << ":" << m.line () << ":" << m.column ()
                     << " error: integer bounds expected in MySQL type "
                     << "declaration" << endl;

                throw generation_failed ();
              }

              unsigned int v;
              istringstream is (t.literal ());

              if (!(is >> v && is.eof ()))
              {
                cerr << m.file () << ":" << m.line () << ":" << m.column ()
                     << " error: invalid bounds value '" << t.literal ()
                     << "'in MySQL type declaration" << endl;

                throw generation_failed ();
              }

              r.bounds = true;
              r.bounds_value = v;

              t = l.next ();

              if (t.punctuation () == sql_token::p_comma)
              {
                // We have the second bounds value. Skip it.
                //
                l.next ();
                t = l.next ();
              }

              if (t.punctuation () != sql_token::p_rparen)
              {
                cerr << m.file () << ":" << m.line () << ":" << m.column ()
                     << " error: expected ')' in MySQL type declaration"
                     << endl;

                throw generation_failed ();
              }

              s = parse_sign;
              continue;
            }

            // Fall through.
            //
            s = parse_sign;
          }
        case parse_sign:
          {
            if (tt == sql_token::t_identifier && t.identifier () == "UNSIGNED")
            {
              r.unsign = true;
            }

            return r;
          }
        }
      }

      cerr << m.file () << ":" << m.line () << ":" << m.column ()
           << " error: empty MySQL type declaration" << endl;

      throw generation_failed ();
    }
    catch (sql_lexer::invalid_input const& e)
    {
      cerr << m.file () << ":" << m.line () << ":" << m.column ()
           << " error: invalid MySQL type declaration: " << e.message << endl;

      throw generation_failed ();
    }
  }
}
