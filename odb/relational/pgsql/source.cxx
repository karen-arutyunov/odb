// file      : odb/relational/pgsql/source.cxx
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

      struct query_parameters: relational::query_parameters
      {
        query_parameters (base const& x)
            : base (x),
              i_ (0)
        {
        }

        virtual string
        next ()
        {
          ostringstream ss;
          ss << "$" << ++i_;

          return ss.str ();
        }

        virtual string
        auto_id ()
        {
          ++i_;
          return "DEFAULT";
        }

      private:
        size_t i_;
      };
      entry<query_parameters> query_parameters_;

      namespace
      {
        const char* integer_buffer_types[] =
        {
          "pgsql::bind::boolean",
          "pgsql::bind::smallint",
          "pgsql::bind::integer",
          "pgsql::bind::bigint"
        };

        const char* float_buffer_types[] =
        {
          "pgsql::bind::real",
          "pgsql::bind::double_"
        };

        const char* char_bin_buffer_types[] =
        {
          "pgsql::bind::text",  // CHAR
          "pgsql::bind::text",  // VARCHAR
          "pgsql::bind::text",  // TEXT
          "pgsql::bind::bytea"  // BYTEA
        };

        const char* date_time_buffer_types[] =
        {
          "pgsql::bind::date",
          "pgsql::bind::time",
          "pgsql::bind::timestamp"
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
      }

      struct statement_oids: object_columns_base, context
      {
        statement_oids (statement_kind sk): sk_ (sk) {}

        virtual bool
        traverse_column (semantics::data_member& m,
                         string const&,
                         bool first)
        {
          // Ignore certain columns depending on what kind statement we are
          // generating. See object_columns in common source generator for
          // details.
          //
          if (inverse (m) && sk_ != statement_select)
            return false;

          if ((id (m) || readonly (member_path_, member_scope_)) &&
              sk_ == statement_update)
            return false;

          if ((sk_ == statement_insert || sk_ == statement_update) &&
              version (m))
            return false;

          if (!first)
            os << ',' << endl;

          os << oids[column_sql_type (m).type];

          return true;
        }

      private:
        statement_kind sk_;
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

            if (inverse (mi.m, key_prefix_) || version (mi.m))
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
            if (inverse (mi.m, key_prefix_) || version (mi.m))
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
             << b << ".capacity = " << arg << "." << mi.var <<
            "value.capacity ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_date_time (member_info& mi)
        {
          os << b << ".type = " <<
            date_time_buffer_types[mi.st->type - sql_type::DATE] << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << b << ".type = " <<
            char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".capacity = " << arg << "." << mi.var <<
            "value.capacity ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_bit (member_info& mi)
        {
          os << b << ".type = pgsql::bind::bit;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = sizeof (" << arg << "." << mi.var << "value);"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_varbit (member_info& mi)
        {
          os << b << ".type = pgsql::bind::varbit;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".capacity = " << arg << "." << mi.var <<
            "value.capacity ();"
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
          if (container (mi))
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
          if (semantics::class_* c = composite (mi.t))
            index_ += column_count (*c).total;
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
          os << e << " = 0;"
             << endl;
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

            traits = "pgsql::value_traits<\n    "
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
              os << "}";

              if (!null (mi.m, key_prefix_))
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
          os << "if (" << traits << "::init (" << endl
             << "i." << mi.var << "value," << endl
             << member << "," << endl
             << "sk))" << endl
             << "grew = true;";
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
        traverse_date_time (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
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
                 << "if (i." << mi.var << "null)" << endl;

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

            traits = "pgsql::value_traits<\n    "
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
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
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
        traverse_date_time (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
             << endl;
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
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
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
        persist_statement_extra (type& c,
                                 relational::query_parameters&,
                                 persist_position p)
        {
          if (p != persist_after_values)
            return;

          semantics::data_member* id (id_member (c));

          if (id != 0 && id->count ("auto"))
          {
            os << endl
               << strlit (" RETURNING " + column_qname (*id));
          }
        }

        virtual void
        object_extra (type& c)
        {
          if (abstract (c))
            return;

          semantics::data_member* id (id_member (c));
          semantics::data_member* optimistic (context::optimistic (c));
          column_count_type const& cc (column_count (c));

          string const& n (class_fq_name (c));
          string traits ("access::object_traits< " + n + " >::");
          string const& fn (flat_name (n));
          string name_decl ("const char " + traits);

          os << name_decl << endl
             << "persist_statement_name[] = " << strlit (fn + "_persist") <<
            ";"
             << endl;

          if (id != 0)
          {
            os << name_decl << endl
               << "find_statement_name[] = " << strlit (fn + "_find") << ";"
               << endl;

            if (cc.total != cc.id + cc.inverse + cc.readonly)
              os << name_decl << endl
                 << "update_statement_name[] = " << strlit (fn + "_update") <<
                ";"
                 << endl;

            os << name_decl << endl
               << "erase_statement_name[] = " << strlit (fn + "_erase") << ";"
               << endl;

            if (optimistic != 0)
              os << name_decl << endl
                 << "optimistic_erase_statement_name[] = " <<
                strlit (fn + "_optimistic_erase") << ";"
                 << endl;
          }

          // Query statement name.
          //
          if (options.generate_query ())
          {
            os << name_decl << endl
               << "query_statement_name[] = " << strlit (fn + "_query") << ";"
               << endl
               << name_decl << endl
               << "erase_query_statement_name[] = " <<
              strlit (fn + "_erase_query") << ";"
               << endl;
          }

          // Statement types.
          //
          string oid_decl ("const unsigned int " + traits);

          // persist_statement_types.
          //
          {
            os << oid_decl << endl
               << "persist_statement_types[] ="
               << "{";

            instance<statement_oids> st (statement_insert);
            st->traverse (c);

            os << "};";
          }

          // find_statement_types.
          //
          if (id != 0)
          {
            os << oid_decl << endl
               << "find_statement_types[] ="
               << "{";

            instance<statement_oids> st (statement_select);
            st->traverse_column (*id, "", true);

            os << "};";
          }

          // update_statement_types.
          //
          if (id != 0 && cc.total != cc.id + cc.inverse + cc.readonly)
          {
            os << oid_decl << endl
               << "update_statement_types[] ="
               << "{";

            {
              instance<statement_oids> st (statement_update);
              st->traverse (c);
            }

            bool first (cc.total == cc.id + cc.inverse + cc.readonly +
                        cc.optimistic_managed);

            {
              instance<statement_oids> st (statement_where);
              st->traverse_column (*id, "", first);
            }

            if (optimistic != 0)
            {
              instance<statement_oids> st (statement_where);
              st->traverse_column (*optimistic, "", false);
            }

            os << "};";
          }

          // erase_statement_types.
          //
          if (id != 0)
          {
            os << oid_decl << endl
               << "erase_statement_types[] ="
               << "{";

            instance<statement_oids> st (statement_where);
            st->traverse_column (*id, "", true);

            os << "};";
          }

          if (id != 0 && optimistic != 0)
          {
            os << oid_decl << endl
               << "optimistic_erase_statement_types[] ="
               << "{";

            {
              instance<statement_oids> st (statement_where);
              st->traverse_column (*id, "", true);
            }

            {
              instance<statement_oids> st (statement_where);
              st->traverse_column (*optimistic, "", false);
            }

            os << "};";
          }
        }

        virtual void
        view_extra (type& c)
        {
          string const& n (class_fq_name (c));
          string traits ("access::view_traits< " + n + " >::");
          string const& fn (flat_name (n));
          string name_decl ("const char " + traits);

          os << name_decl << endl
             << "query_statement_name[] = " << strlit (fn + "_query") << ";"
             << endl;
        }

        virtual void
        object_query_statement_ctor_args (type&)
        {
          os << "sts.connection ()," << endl
             << "query_statement_name," << endl
             << "query_statement + q.clause ()," << endl
             << "q.parameter_types ()," << endl
             << "q.parameter_count ()," << endl
             << "q.parameters_binding ()," << endl
             << "imb";
        }

        virtual void
        object_erase_query_statement_ctor_args (type&)
        {
          os << "conn," << endl
             << "erase_query_statement_name," << endl
             << "erase_query_statement + q.clause ()," << endl
             << "q.parameter_types ()," << endl
             << "q.parameter_count ()," << endl
             << "q.parameters_binding ()";
        }

        virtual void
        view_query_statement_ctor_args (type&)
        {
          os << "sts.connection ()," << endl
             << "query_statement_name," << endl
             << "qs.clause ()," << endl
             << "qs.parameter_types ()," << endl
             << "qs.parameter_count ()," << endl
             << "qs.parameters_binding ()," << endl
             << "imb";
        }

        virtual void
        post_query_ (type&)
        {
          os << "st->deallocate ();";
        }
      };
      entry<class_> class_entry_;

      struct container_traits : relational::container_traits, context
      {
        container_traits (base const& x): base (x) {}

        virtual void
        container_extra (semantics::data_member& m, semantics::type& t)
        {
          if (!object (c_) || abstract (c_))
            return;

          string scope (scope_ + "::" + flat_prefix_ + public_name (m) +
                        "_traits");

          // Statment names.
          //
          string stmt_decl ("const char " + scope + "::");

          // Prefix top-object name to avoid conflicts with inherited
          // member statement names.
          //
          string stmt_prefix (class_fq_name (*top_object) +  m.fq_name ());

          os << stmt_decl << endl
             << "select_all_name[] = " <<
             strlit (stmt_prefix + "_select_all") << ";"
             << endl
             << stmt_decl << endl
             << "insert_one_name[] = " <<
             strlit (stmt_prefix + "_insert_one") << ";"
             << endl
             << stmt_decl << endl
             << "delete_all_name[] = " <<
             strlit (stmt_prefix + "_delete_all") << ";"
             << endl;

          // Statement types.
          //
          string type_decl ("const unsigned int " + scope + "::");

          semantics::data_member* inv_m (inverse (m, "value"));
          bool inv (inv_m != 0);

          semantics::type& vt (container_vt (t));

          string id_oid (oids[column_sql_type (m, "id").type]);

          // select_all statement types.
          //
          {
            os << type_decl << endl
               << "select_all_types[] ="
               << "{";

            if (inv)
            {
              // many(i)-to-many
              //
              if (container (*inv_m))
                os << oids[column_sql_type (*inv_m, "value").type];

              // many(i)-to-one
              //
              else
                os << oids[column_sql_type (*inv_m).type];
            }
            else
              os << id_oid;

            os << "};";
          }

          // insert_one statement types.
          //
          {
            os << type_decl << endl
               << "insert_one_types[] ="
               << "{";

            if (!inv)
            {
              os << id_oid << ",";

              switch (container_kind (t))
              {
              case ck_ordered:
                {
                  if (!unordered (m))
                    os << oids[column_sql_type (m, "index").type] << ",";

                  break;
                }
              case ck_map:
              case ck_multimap:
                {
                  if (semantics::class_* ktc =
                      composite_wrapper (container_kt (t)))
                  {
                    instance<statement_oids> st (statement_insert);
                    st->traverse (m, *ktc, "key", "key");
                    os << ",";
                  }
                  else
                    os << oids[column_sql_type (m, "key").type] << ",";

                  break;
                }
              case ck_set:
              case ck_multiset:
                {
                  break;
                }
              }

              if (semantics::class_* vtc = composite_wrapper (vt))
              {
                instance <statement_oids> st (statement_insert);
                st->traverse (m, *vtc, "value", "value");
              }
              else
                os << oids[column_sql_type (m, "value").type];

            }
            else
              // MSVC does not allow zero length arrays or uninitialized
              // non-extern const values.
              //
              os << "0";

            os << "};";
          }

          // delete_all statement types.
          //
          {
            os << type_decl << endl
               << "delete_all_types[] ="
               << "{";

            if (!inv)
              os << id_oid;
            else
              os << "0";

            os << "};";
          }
        }
      };
      entry<container_traits> container_traits_;
    }
  }
}
