// file      : odb/relational/sqlite/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>
#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/relational/sqlite/context.hxx>
#include <odb/relational/sqlite/common.hxx>

using namespace std;

namespace relational
{
  namespace sqlite
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
        {"bool", "INTEGER", 0},

        {"char", "INTEGER", 0},
        {"signed char", "INTEGER", 0},
        {"unsigned char", "INTEGER", 0},

        {"short int", "INTEGER", 0},
        {"short unsigned int", "INTEGER", 0},

        {"int", "INTEGER", 0},
        {"unsigned int", "INTEGER", 0},

        {"long int", "INTEGER", 0},
        {"long unsigned int", "INTEGER", 0},

        {"long long int", "INTEGER", 0},
        {"long long unsigned int", "INTEGER", 0},

        {"float", "REAL", 0},
        {"double", "REAL", 0},

        {"::std::string", "TEXT", 0}
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
    context (ostream& os, semantics::unit& u, options_type const& ops)
        : root_context (os, u, ops, data_ptr (new (shared) data (os))),
          base_context (static_cast<data*> (root_context::data_.get ())),
          data_ (static_cast<data*> (base_context::data_))
    {
      assert (current_ == 0);
      current_ = this;

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

    namespace
    {
      struct has_grow: traversal::class_
      {
        has_grow (bool& r)
            : r_ (r)
        {
          *this >> inherits_ >> *this;
        }

        virtual void
        traverse (type& c)
        {
          // Ignore transient bases.
          //
          if (!(c.count ("object") || context::comp_value (c)))
            return;

          if (c.count ("sqlite-grow"))
            r_ = c.get<bool> ("sqlite-grow");
          else
          {
            // r_ should be false.
            //
            inherits (c);

            if (!r_)
              names (c);

            c.set ("sqlite-grow", r_);
          }
        }

      private:
        bool& r_;
        traversal::inherits inherits_;
      };

      struct has_grow_member: member_base
      {
        has_grow_member (bool& r,
                         semantics::type* type = 0,
                         string const& key_prefix = string ())
            : relational::member_base (type, string (), key_prefix),
              r_ (r)
        {
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          // By calling grow() instead of recursing, we reset any overrides.
          //
          r_ = r_ || context::grow (dynamic_cast<semantics::class_&> (mi.t));
        }

        virtual void
        traverse_decimal (member_info&)
        {
          r_ = true;
        }

        virtual void
        traverse_long_string (member_info&)
        {
          r_ = true;
        }

        virtual void
        traverse_short_string (member_info&)
        {
          r_ = true; // @@ Short string optimization disabled.
        }

        virtual void
        traverse_enum (member_info&)
        {
          r_ = true;
        }

        virtual void
        traverse_set (member_info&)
        {
          r_ = true;
        }

      private:
        bool& r_;
      };
    }

    bool context::
    grow_impl (semantics::class_& c)
    {
      if (c.count ("sqlite-grow"))
        return c.get<bool> ("sqlite-grow");

      bool r (false);
      has_grow ct (r);
      has_grow_member mt  (r);
      traversal::names names;
      ct >> names >> mt;
      ct.traverse (c);
      return r;
    }

    bool context::
    grow_impl (semantics::data_member& m)
    {
      bool r (false);
      has_grow_member mt  (r);
      mt.traverse (m);
      return r;
    }

    bool context::
    grow_impl (semantics::data_member& m, semantics::type& t, string const& kp)
    {
      bool r (false);
      has_grow_member mt  (r, &t, kp);
      mt.traverse (m);
      return r;
    }

    //
    // SQL type parsing.
    //

    static sql_type
    parse_sql_type (semantics::data_member& m, std::string const& sql);

    sql_type const& context::
    column_sql_type (semantics::data_member& m, string const& kp)
    {
      string key (kp.empty ()
                  ? string ("sqlite-column-sql-type")
                  : "sqlite-" + kp + "-column-sql-type");

      if (!m.count (key))
        m.set (key, parse_sql_type (m, column_type (m, kp)));

      return m.get<sql_type> (key);
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
          parse_range,
          parse_sign,
          parse_done
        };

        state s (parse_prefix);
        string prefix;

        for (sql_token t (l.next ());
             s != parse_done && t.type () != sql_token::t_eos;
             t = l.next ())
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
                  prefix = id;
                  s = parse_name;
                  continue;
                }
              }

              // Fall through.
              //
              s = parse_name;
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
                  r.type = sql_type::BIT;
                }
                else if (id == "TINYINT" || id == "INT1")
                {
                  r.type = sql_type::TINYINT;
                }
                else if (id == "BOOL" || id == "BOOLEAN")
                {
                  r.type = sql_type::TINYINT;
                  r.range = true;
                  r.range_value = 1;
                }
                else if (id == "SMALLINT" || id == "INT2")
                {
                  r.type = sql_type::SMALLINT;
                }
                else if (id == "MEDIUMINT" ||
                         id == "INT3" ||
                         id == "MIDDLEINT")
                {
                  r.type = sql_type::MEDIUMINT;
                }
                else if (id == "INT" || id == "INTEGER" || id == "INT4")
                {
                  r.type = sql_type::INT;
                }
                else if (id == "BIGINT" || id == "INT8")
                {
                  r.type = sql_type::BIGINT;
                }
                else if (id == "SERIAL")
                {
                  r.type = sql_type::BIGINT;
                  r.unsign = true;
                }
                else if (id == "FLOAT" || id == "FLOAT4")
                {
                  r.type = sql_type::FLOAT;
                }
                else if (id == "DOUBLE" || id == "FLOAT8")
                {
                  r.type = sql_type::DOUBLE;
                }
                else if (id == "DECIMAL" ||
                         id == "DEC" ||
                         id == "NUMERIC" ||
                         id == "FIXED")
                {
                  r.type = sql_type::DECIMAL;
                }
                //
                // Date-time types.
                //
                else if (id == "DATE")
                {
                  r.type = sql_type::DATE;
                }
                else if (id == "TIME")
                {
                  r.type = sql_type::TIME;
                }
                else if (id == "DATETIME")
                {
                  r.type = sql_type::DATETIME;
                }
                else if (id == "TIMESTAMP")
                {
                  r.type = sql_type::TIMESTAMP;
                }
                else if (id == "YEAR")
                {
                  r.type = sql_type::YEAR;
                }
                //
                // String and binary types.
                //
                else if (id == "NCHAR")
                {
                  r.type = sql_type::CHAR;
                }
                else if (id == "VARCHAR")
                {
                  r.type = prefix == "LONG"
                    ? sql_type::MEDIUMTEXT
                    : sql_type::VARCHAR;
                }
                else if (id == "NVARCHAR")
                {
                  r.type = sql_type::VARCHAR;
                }
                else if (id == "VARYING" && prefix == "CHARACTER")
                {
                  r.type = sql_type::VARCHAR;
                }
                else if (id == "BINARY")
                {
                  r.type = sql_type::BINARY;
                }
                else if (id == "BYTE" && prefix == "CHAR")
                {
                  r.type = sql_type::BINARY;
                }
                else if (id == "VARBINARY")
                {
                  r.type = prefix == "LONG"
                    ? sql_type::MEDIUMBLOB
                    : sql_type::VARBINARY;
                }
                else if (id == "TINYBLOB")
                {
                  r.type = sql_type::TINYBLOB;
                }
                else if (id == "TINYTEXT")
                {
                  r.type = sql_type::TINYTEXT;
                }
                else if (id == "BLOB")
                {
                  r.type = sql_type::BLOB;
                }
                else if (id == "TEXT")
                {
                  r.type = sql_type::TEXT;
                }
                else if (id == "MEDIUMBLOB")
                {
                  r.type = sql_type::MEDIUMBLOB;
                }
                else if (id == "MEDIUMTEXT")
                {
                  r.type = sql_type::MEDIUMTEXT;
                }
                else if (id == "LONGBLOB")
                {
                  r.type = sql_type::LONGBLOB;
                }
                else if (id == "LONGTEXT")
                {
                  r.type = sql_type::LONGTEXT;
                }
                else if (id == "ENUM")
                {
                  r.type = sql_type::ENUM;
                }
                else if (id == "SET")
                {
                  r.type = sql_type::SET;
                }
                else
                  match = false;

                if (match)
                {
                  s = parse_range;
                  continue;
                }
              }

              // Some prefixes can also be type names if not followed
              // by the actual type name.
              //
              if (!prefix.empty ())
              {
                if (prefix == "CHAR" || prefix == "CHARACTER")
                {
                  r.type = sql_type::CHAR;
                }
                else if (prefix == "LONG")
                {
                  r.type = sql_type::MEDIUMTEXT;
                }
              }

              if (r.type == sql_type::invalid)
              {
                cerr << m.file () << ":" << m.line () << ":" <<
                  m.column () << ":";

                if (tt == sql_token::t_identifier)
                  cerr << " error: unknown MySQL type '" <<
                    t.identifier () << "'" << endl;
                else
                  cerr << " error: expected MySQL type name" << endl;

                throw generation_failed ();
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

                // ENUM and SET have a list of members instead of the range.
                //
                if (r.type == sql_type::ENUM || r.type == sql_type::SET)
                {
                  // Skip tokens until we get the closing paren.
                  //
                  while (t.type () != sql_token::t_eos &&
                         t.punctuation () != sql_token::p_rparen)
                    t = l.next ();
                }
                else
                {
                  if (t.type () != sql_token::t_int_lit)
                  {
                    cerr << m.file () << ":" << m.line () << ":" << m.column ()
                         << ": error: integer range expected in MySQL type "
                         << "declaration" << endl;

                    throw generation_failed ();
                  }

                  unsigned int v;
                  istringstream is (t.literal ());

                  if (!(is >> v && is.eof ()))
                  {
                    cerr << m.file () << ":" << m.line () << ":" << m.column ()
                         << ": error: invalid range value '" << t.literal ()
                         << "'in MySQL type declaration" << endl;

                    throw generation_failed ();
                  }

                  r.range = true;
                  r.range_value = v;

                  t = l.next ();

                  if (t.punctuation () == sql_token::p_comma)
                  {
                    // We have the second range value. Skip it.
                    //
                    l.next ();
                    t = l.next ();
                  }
                }

                if (t.punctuation () != sql_token::p_rparen)
                {
                  cerr << m.file () << ":" << m.line () << ":" << m.column ()
                       << ": error: expected ')' in MySQL type declaration"
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
              if (tt == sql_token::t_identifier &&
                  t.identifier () == "UNSIGNED")
              {
                r.unsign = true;
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

        if (s == parse_name && !prefix.empty ())
        {
          // Some prefixes can also be type names if not followed
          // by the actual type name.
          //
          if (prefix == "CHAR" || prefix == "CHARACTER")
          {
            r.type = sql_type::CHAR;
          }
          else if (prefix == "LONG")
          {
            r.type = sql_type::MEDIUMTEXT;
          }
        }

        if (r.type == sql_type::invalid)
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: incomplete MySQL type declaration" << endl;

          throw generation_failed ();
        }

        // If range is omitted for CHAR or BIT types, it defaults to 1.
        //
        if ((r.type == sql_type::CHAR || r.type == sql_type::BIT) && !r.range)
        {
          r.range = true;
          r.range_value = 1;
        }

        return r;
      }
      catch (sql_lexer::invalid_input const& e)
      {
        cerr << m.file () << ":" << m.line () << ":" << m.column ()
             << ": error: invalid MySQL type declaration: " << e.message
             << endl;

        throw generation_failed ();
      }
    }
  }
}
