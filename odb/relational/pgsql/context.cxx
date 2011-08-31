// file      : odb/relational/pgsql/context.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>
#include <sstream>

#include <odb/sql-token.hxx>
#include <odb/sql-lexer.hxx>

#include <odb/relational/pgsql/context.hxx>
#include <odb/relational/pgsql/common.hxx>

using namespace std;

namespace relational
{
  namespace pgsql
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
        {"bool", "BOOLEAN", 0},

        {"char", "SMALLINT", 0},
        {"signed char", "SMALLINT", 0},
        {"unsigned char", "SMALLINT", 0},

        {"short int", "SMALLINT", 0},
        {"short unsigned int", "SMALLINT", 0},

        {"int", "INTEGER", 0},
        {"unsigned int", "INTEGER", 0},

        {"long int", "BIGINT", 0},
        {"long unsigned int", "BIGINT", 0},

        {"long long int", "BIGINT", 0},
        {"long long unsigned int", "BIGINT", 0},

        {"float", "REAL", 0},
        {"double", "DOUBLE PRECISION", 0},

        {"::std::string", "TEXT", 0},

        {"::size_t", "BIGINT", 0},
        {"::std::size_t", "BIGINT", 0}
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

      data_->bind_vector_ = "pgsql::bind*";
      data_->truncated_vector_ = "bool*";

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
          if (!(context::object (c) || context::composite (c)))
            return;

          if (c.count ("pgsql-grow"))
            r_ = c.get<bool> ("pgsql-grow");
          else
          {
            // r_ should be false.
            //
            inherits (c);

            if (!r_)
              names (c);

            c.set ("pgsql-grow", r_);
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
        traverse_numeric (member_info&)
        {
          r_ = true;
        }

        virtual void
        traverse_string (member_info&)
        {
          r_ = true;
        }

        virtual void
        traverse_varbit (member_info&)
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
      if (c.count ("pgsql-grow"))
        return c.get<bool> ("pgsql-grow");

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

    string context::
    database_type_impl (semantics::type& t, semantics::names* hint, bool id)
    {
      string r (base_context::database_type_impl (t, hint, id));

      if (!r.empty ())
        return r;

      using semantics::enum_;

      if (t.is_a<semantics::enum_> ())
        r = "INTEGER";

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
                  ? string ("pgsql-column-sql-type")
                  : "pgsql-" + kp + "-column-sql-type");

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
        // BIT VARYING               (VARBIT)
        // CHARACTER VARYING         (VARRCHAR)
        // DOUBLE PRECISION          (DOUBLE)
        // TIME WITH TIME ZONE       (not currently supported)
        // TIMESTAMP WITH TIME ZONE  (not currently supported)
        //

        enum state
        {
          parse_prefix,
          parse_name,
          parse_range,
          parse_suffix,
          parse_done
        };

        state s (parse_prefix);
        string prefix;
        bool flt (false);

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
                string const& id (context::upcase (t.identifier ()));

                if (id == "BIT" ||
                    id == "CHARACTER" ||
                    id == "DOUBLE")
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
                string const& id (context::upcase (t.identifier ()));

                //
                // Numeric types.
                //
                if (id == "BOOL" || id == "BOOLEAN")
                {
                  r.type = sql_type::BOOLEAN;
                }
                else if (id == "SMALLINT" || id == "INT2")
                {
                  r.type = sql_type::SMALLINT;
                }
                else if (id == "INT" ||
                         id == "INTEGER" ||
                         id == "INT4")
                {
                  r.type = sql_type::INTEGER;
                }
                else if (id == "BIGINT")
                {
                  r.type = sql_type::BIGINT;
                }
                else if (id == "REAL" || id == "FLOAT4")
                {
                  r.type = sql_type::REAL;
                }
                else if ((id == "PRECISION" && prefix == "DOUBLE") ||
                         id == "FLOAT8")
                {
                  r.type = sql_type::DOUBLE;
                }
                else if (id == "FLOAT")
                {
                  // Assign a type only once we know the precision of the
                  // float.
                  //
                  flt = true;
                }
                else if (id == "NUMERIC" || id == "DECIMAL")
                {
                  r.type = sql_type::NUMERIC;
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
                else if (id == "TIMETZ")
                {
                  cerr << m.file () << ":" << m.line () << ":" << m.column ()
                       << ": error: PostgreSQL time zones are not currently "
                       << "supported" << endl;

                  throw generation_failed ();
                }
                else if (id == "TIMESTAMP")
                {
                  r.type = sql_type::TIMESTAMP;
                }
                else if (id == "TIMESTAMPTZ")
                {
                  cerr << m.file () << ":" << m.line () << ":" << m.column ()
                       << ": error: PostgreSQL time zones are not currently "
                       << "supported" << endl;

                  throw generation_failed ();
                }
                //
                // String and binary types.
                //
                else if (id == "CHAR")
                {
                  r.type = sql_type::CHAR;
                }
                else if (id == "VARCHAR")
                {
                  r.type = sql_type::VARCHAR;
                }
                else if (id == "TEXT")
                {
                  r.type = sql_type::TEXT;
                }
                else if (id == "VARYING")
                {
                  if (prefix == "BIT")
                    r.type = sql_type::VARBIT;
                  else if (prefix == "CHARACTER")
                    r.type = sql_type::VARCHAR;
                }
                else if (id == "BYTEA")
                {
                  r.type = sql_type::BYTEA;
                }
                else if (id == "VARBIT")
                {
                  r.type = sql_type::VARBIT;
                }
                //
                // Other types.
                //
                else if (id == "UUID")
                {
                  r.type = sql_type::UUID;
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
                if (prefix == "BIT")
                {
                  r.type = sql_type::BIT;
                }
                else if (prefix == "CHARACTER")
                {
                  r.type = sql_type::CHAR;
                }
              }

              if (r.type == sql_type::invalid)
              {
                cerr << m.file () << ":" << m.line () << ":" <<
                  m.column () << ":";

                if (tt == sql_token::t_identifier)
                  cerr << " error: unknown PostgreSQL type '" <<
                    t.identifier () << "'" << endl;
                else
                  cerr << " error: expected PostgreSQL type name" << endl;

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

                if (t.type () != sql_token::t_int_lit)
                {
                  cerr << m.file () << ":" << m.line () << ":" << m.column ()
                       << ": error: integer range expected in PostgreSQL type "
                       << "declaration" << endl;

                  throw generation_failed ();
                }

                unsigned int v;
                istringstream is (t.literal ());

                if (!(is >> v && is.eof ()))
                {
                  cerr << m.file () << ":" << m.line () << ":" << m.column ()
                       << ": error: invalid range value '" << t.literal ()
                       << "'in PostgreSQL type declaration" << endl;

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

                if (t.punctuation () != sql_token::p_rparen)
                {
                  cerr << m.file () << ":" << m.line () << ":" << m.column ()
                       << ": error: expected ')' in PostgreSQL type "
                       << "declaration" << endl;

                  throw generation_failed ();
                }

                s = parse_suffix;
                continue;
              }

              // Fall through.
              //
              s = parse_suffix;
            }
          case parse_suffix:
            {
              if (r.type == sql_type::TIME || r.type == sql_type::TIMESTAMP)
              {
                string const& id1 (context::upcase (t.identifier ()));

                if (id1 == "WITH")
                {
                  t = l.next ();
                  tt = t.type ();

                  if (tt == sql_token::t_identifier)
                  {
                    string const& id2 (context::upcase (t.identifier ()));

                    if (id2 == "TIME")
                    {
                      t = l.next ();
                      tt = t.type ();

                      if (tt == sql_token::t_identifier)
                      {
                        string const& id3 (context::upcase (t.identifier ()));

                        if (id3 == "ZONE")
                        {
                          cerr << m.file () << ":" << m.line () << ":"
                               << m.column ()<< ": error: PostgreSQL time "
                               << "zones are not currently supported" << endl;

                          throw generation_failed ();
                        }
                      }
                    }
                  }
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

        if (s == parse_name && !prefix.empty ())
        {
          // Some prefixes can also be type names if not followed
          // by the actual type name.
          //
          if (prefix == "BIT")
          {
            r.type = sql_type::BIT;
          }
          else if (prefix == "CHARACTER")
          {
            r.type = sql_type::CHAR;
          }
        }

        if (flt)
        {
          r.type = r.range && r.range_value < 25 ?
            sql_type::REAL :
            sql_type::DOUBLE;
        }

        if (r.type == sql_type::invalid)
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: incomplete PostgreSQL type declaration" << endl;

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
             << ": error: invalid PostgreSQL type declaration: " << e.message
             << endl;

        throw generation_failed ();
      }
    }
  }
}
