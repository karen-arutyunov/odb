// file      : odb/relational/sqlite/context.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <vector>
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

      data_->bind_vector_ = "sqlite::bind*";
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
        traverse_string (member_info&)
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

    namespace
    {
      struct sql_parser
      {
        sql_parser (semantics::data_member& m, std::string const& sql)
            : m_ (m), l_ (sql)
        {
        }

        // Issues diagnostics and throws generation_failed in case of
        // an error.
        //
        sql_type
        parse ()
        {
          try
          {
            for (sql_token t (l_.next ()); t.type () != sql_token::t_eos;)
            {
              sql_token::token_type tt (t.type ());

              if (tt == sql_token::t_identifier)
              {
                string const& id (context::upcase (t.identifier ()));

                // Column constraints start with one of the following
                // keywords. Use them to determine when to stop parsing.
                //
                if (id == "CONSTRAINT" ||
                    id == "PRIMARY" ||
                    id == "NOT" ||
                    id == "UNIQUE" ||
                    id == "CHECK" ||
                    id == "DEFAULT" ||
                    id == "COLLATE" ||
                    id == "REFERENCES")
                {
                  break;
                }

                ids_.push_back (id);
                t = l_.next ();

                if (t.punctuation () == sql_token::p_lparen)
                {
                  parse_range ();
                  t = l_.next ();
                }
              }
              else
              {
                cerr << m_.file () << ":" << m_.line () << ":" << m_.column ()
                     << ": error: expected SQLite type name instead of '"
                     << t << "'" << endl;
                throw generation_failed ();
              }
            }
          }
          catch (sql_lexer::invalid_input const& e)
          {
            cerr << m_.file () << ":" << m_.line () << ":" << m_.column ()
                 << ": error: invalid SQLite type declaration: " << e.message
                 << endl;
            throw generation_failed ();
          }

          if (ids_.empty ())
          {
            cerr << m_.file () << ":" << m_.line () << ":" << m_.column ()
                 << ": error: expected SQLite type name" << endl;
            throw generation_failed ();
          }

          sql_type r;

          // Apply the first four rules of the SQLite type to affinity
          // conversion algorithm.
          //
          if (find ("INT"))
            r.type = sql_type::INTEGER;
          else if (find ("TEXT") || find ("CHAR") || find ("CLOB"))
            r.type = sql_type::TEXT;
          else if (find ("BLOB"))
            r.type = sql_type::BLOB;
          else if (find ("REAL") || find ("FLOA") || find ("DOUB"))
            r.type = sql_type::REAL;
          else
          {
            // Instead of the fifth rule which maps everything else
            // to NUMERICAL (which we don't have), map some commonly
            // used type names to one of the above types.
            //
            string const& id (ids_[0]);

            if (id == "NUMERIC")
              r.type = sql_type::REAL;
            else if (id == "DECIMAL")
              r.type = sql_type::TEXT;
            else if (id == "BOOLEAN" || id == "BOOL")
              r.type = sql_type::INTEGER;
            else if (id == "DATE" || id == "TIME" || id == "DATETIME")
              r.type = sql_type::TEXT;
            else
            {
              cerr << m_.file () << ":" << m_.line () << ":" << m_.column ()
                   << " error: unknown SQLite type '" << id << "'" << endl;
              throw generation_failed ();
            }
          }

          return r;
        }

        void
        parse_range ()
        {
          // Skip tokens until we get the closing paren.
          //
          for (sql_token t (l_.next ());; t = l_.next ())
          {
            if (t.punctuation () == sql_token::p_rparen)
              break;

            if (t.type () == sql_token::t_eos)
            {
              cerr << m_.file () << ":" << m_.line () << ":" << m_.column ()
                   << ": error: missing ')' in SQLite type declaration"
                   << endl;
              throw generation_failed ();
            }
          }
        }

        bool
        find (string const& str) const
        {
          for (identifiers::const_iterator i (ids_.begin ());
               i != ids_.end (); ++i)
          {
            if (i->find (str) != string::npos)
              return true;
          }

          return false;
        }

      private:
        typedef vector<string> identifiers;

      private:
        semantics::data_member& m_;
        sql_lexer l_;
        identifiers ids_;
      };
    }

    sql_type const& context::
    column_sql_type (semantics::data_member& m, string const& kp)
    {
      string key (kp.empty ()
                  ? string ("sqlite-column-sql-type")
                  : "sqlite-" + kp + "-column-sql-type");

      if (!m.count (key))
      {
        sql_parser p (m, column_type (m, kp));
        m.set (key, p.parse ());
      }

      return m.get<sql_type> (key);
    }
  }
}
