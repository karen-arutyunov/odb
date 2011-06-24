// file      : odb/relational/pgsql/source.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <sstream>

#include <odb/relational/source.hxx>

#include <odb/relational/pgsql/common.hxx>
#include <odb/relational/pgsql/context.hxx>

using namespace std;

namespace relational
{
  namespace pgsql
  {
    namespace source
    {
      namespace relational = relational::source;

      struct query_parameters: context
      {
        query_parameters ()
            : i_ (0)
        {
        }

        virtual string
        next ()
        {
          ostringstream ss ("$");
          ss << ++i_;

          return ss.str ();
        }

      private:
        size_t i_;
      };

      namespace
      {
        const char* integer_buffer_types[] =
        {
          "pgsql::bind::boolean",
          "pgsql::bind::smallint",
          "pgsql::bind::integer",
          "pgsql::bind::bigint",
        };

        const char* float_buffer_types[] =
        {
          "pgsql::bind::real",
          "pgsql::bind::double_",
        };

        const char* char_bin_buffer_types[] =
        {
          "pgsql::bind::text",  // CHAR
          "pgsql::bind::text",  // VARCHAR
          "pgsql::bind::text",  // TEXT
          "pgsql::bind::bytea", // BYTEA
        };

        const char* oids[] =
        {
          "pgsql::bool_oid",      // BOOLEAN
          "pgsql::int2_oid",      // SMALLINT
          "pgsql::int4_oid",      // INTEGER
          "pgsql::int8_oid",      // BIGINT
          "pgsql::float4_oid",    // REAL
          "pgsql::float8_oid",    // DOUBLE
          "pgsql::numeric_oid",   // NUMERIC
          "pgsql::date_oid",      // DATE
          "pgsql::time_oid",      // TIME
          "pgsql::timestamp_oid", // TIMESTAMP
          "pgsql::text_oid",      // CHAR
          "pgsql::text_oid",      // VARCHAR
          "pgsql::text_oid",      // TEXT
          "pgsql::bytea_oid",     // BYTEA
          "pgsql::bit_oid",       // BIT
          "pgsql::varbit_oid",    // VARBIT
          "pgsql::uuid_oid"       // UUID
        };

        // @@ Complete once date-time format is known.
        //

        // const char* date_time_buffer_types[] =
        // {
        // };
      }

      //
      // statement oids
      //

      struct statement_oids: object_columns_base, context
      {
        statement_oids (ostringstream& os)
            : column_oids (os)
        {
        }

        virtual bool
        column (semantics::data_member& m, std::string const&, bool first)
        {
          semantics::data_member* im (inverse (m));

          if (im != 0)
            return false;

          const sql_type& t (column_sql_type (m));

          if (!first)
            column_oids << ',' << endl;

          column_oids << oids[t.type - sql_type::BOOLEAN];

          if (m.count ("id"))
            id_oid = oids[t.type - sql_type::BOOLEAN];

          return true;
        }

      public:
        string id_oid;

      private:
        ostringstream& column_oids;
      };

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

            if (inverse (mi.m, key_prefix_))
              os << "if (out)"
                 << "{";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (var_override_.empty ())
          {
            if (semantics::class_* c = comp_value (mi.t))
              os << "n += " << in_column_count (*c) << "UL;";
            else
              os << "n++;";

            if (inverse (mi.m, key_prefix_))
              os << "}";
            else
              os << endl;
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "composite_value_traits< " << mi.fq_type () <<
            " >::bind (b + n, " << arg << "." << mi.var << "value);";
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << b << ".type = " <<
            integer_buffer_types[mi.st->type - sql_type::BOOLEAN] << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << b << ".type = " <<
            float_buffer_types[mi.st->type - sql_type::REAL] << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_numeric (member_info& mi)
        {
          os << b << ".type = pgsql::bind::numeric;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".capacity = " << endl
             << arg << "." << mi.var << "value.capacity ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_date_time (member_info&)
        {
          // @@ Complete once date-time format is known.
          //
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << b << ".type = " <<
            char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".capacity = " << endl
             << arg << "." << mi.var << "value.capacity ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_bit (member_info& mi)
        {
          os << b << ".type = pgsql::bind::bit;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = sizeof (" << endl
             << arg << "." << mi.var << "value);"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_varbit (member_info& mi)
        {
          os << b << ".type = pgsql::bind::varbit;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".capacity = " << endl
             << arg << "." << mi.var << "value.capacity ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_uuid (member_info& mi)
        {
          os << b << ".type = pgsql::bind::uuid;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
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
          if (semantics::class_* c = comp_value (mi.t))
            index_ += in_column_count (*c);
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
        traverse_numeric (member_info& mi)
        {
          os << "if (" << e << ")" << endl
             << "{"
             << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
             << "grew = true;"
             << "}";
        }

        virtual void
        traverse_date_time (member_info&)
        {
          // @@ Complete once date-time format is known.
          //
        }

        virtual void
        traverse_string (member_info& mi)
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
        traverse_varbit (member_info& mi)
        {
          os << "if (" << e << ")" << endl
             << "{"
             << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
             << "grew = true;"
             << "}";
        }

        virtual void
        traverse_uuid (member_info&)
        {
          os << e << " = 0;"
             << endl;
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
          // object pointers (they are not present in the 'in' binding).
          //
          if (container (mi.t) || inverse (mi.m, key_prefix_))
            return false;

          if (!member_override_.empty ())
            member = member_override_;
          else
          {
            string const& name (mi.m.name ());
            member = "o." + name;

            os << "// " << name << endl
               << "//" << endl;
          }

          if (comp_value (mi.t))
            traits = "composite_value_traits< " + mi.fq_type () + " >";
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_type (mi.m, key_prefix_));

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

            traits = "pgsql::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (!comp_value (mi.t))
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            if (object_pointer (member_type (mi.m, key_prefix_)))
            {
              os << "}";

              if (!null_pointer (mi.m, key_prefix_))
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
          os << "if (" << traits << "::init (i." << mi.var << "value, " <<
            member << "))"
             << "{"
             << "grew = true;"
             << "}";
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
        traverse_numeric (member_info& mi)
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
             << "i." << mi.var << "size = size;"
             << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
        }

        virtual void
        traverse_date_time (member_info&)
        {
          // @@ Complete once date-time format is known.
          //
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << "std::size_t size (0);"
             << "std::size_t cap (i." << mi.var << "value.capacity ());"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = size;"
             << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
        }

        virtual void
        traverse_bit (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "sizeof (i." << mi.var << "value)," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = size;";
        }

        virtual void
        traverse_varbit (member_info& mi)
        {
          os << "std::size_t size (0);"
             << "std::size_t cap (i." << mi.var << "value.capacity ());"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = size;"
             << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
        }

        virtual void
        traverse_uuid (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
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

            os << "// " << name << endl
               << "//" << endl;
          }

          if (comp_value (mi.t))
            traits = "composite_value_traits< " + mi.fq_type () + " >";
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_type (mi.m, key_prefix_));

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

              if (null_pointer (mi.m, key_prefix_))
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

            traits = "pgsql::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (comp_value (mi.t))
            return;

          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          semantics::type& mt (member_type (mi.m, key_prefix_));

          if (object_pointer (mt))
          {
            member = member_override_.empty ()
              ? "o." + mi.m.name ()
              : member_override_;

            if (lazy_pointer (mt))
              os << member << " = ptr_traits::pointer_type (db, id);";
            else
              os << "// If a compiler error points to the line below, then" << endl
                 << "// it most likely means that a pointer used in a member" << endl
                 << "// cannot be initialized from an object pointer." << endl
                 << "//" << endl
                 << member << " = ptr_traits::pointer_type (" << endl
                 << "db.load< ptr_traits::element_type > (id));";

            os << "}"
               << "}";
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << traits << "::init (" << member << ", i." <<
            mi.var << "value, db);"
             << endl;
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << ", i." << mi.var << "value, " <<
            "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << ", i." << mi.var << "value, " <<
            "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_numeric (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_date_time (member_info&)
        {
          // @@ Complete once date-time format is known.
          //
        }

        virtual void
        traverse_string (member_info& mi)
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
          // Presented as byte.
          //
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_varbit (member_info& mi)
        {
          // Presented as bytea.
          //
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_uuid (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << ", i." << mi.var << "value, " <<
            "i." << mi.var << "null);"
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

        virtual void
        object_extra (type& t)
        {
          if (abstract (t))
            return;

          string const& type (t.fq_name ());
          string traits ("access::object_traits< " + type + " >::");

          //
          // Statement names.
          //

          string fn (flat_name (type));
          string name_decl ("const char* const " + traits);

          os << name_decl << "persist_statement_name =" << endl
             << strlit ( fn + "_persist" ) << ";"
             << name_decl << "find_statement_name =" << endl
             << strlit ( fn + "_find" ) << ";"
             << name_decl << "update_statement_name =" << endl
             << strlit ( fn + "_update" ) << ";"
             << name_decl << "erase_statement_name =" << endl
             << strlit ( fn + "_erase" ) << ";"
             << endl;

          //
          // Statement types.
          //

          ostringstream ss;
          instance<statement_oids> st (ss);
          st->traverse (t);

          string oid_decl ("const Oid " + traits);

          os << oid_decl << endl
             << "persist_statement_types[] ="
             << "{" << ss.str () << "};"
             << endl;

          os << oid_decl << "find_statement_types[] ="
             << "{" << st->id_oid << "};"
             << endl;

          os << oid_decl << "update_statement_types[] ="
             << "{" << ss.str () << "," << endl
             << st->id_oid << "};"
             << endl;

          os << oid_decl << "erase_statement_types[] ="
             << "{" << st->id_oid << "};"
             << endl;
        }
      };
      entry<class_> class_entry_;
    }
  }
}
