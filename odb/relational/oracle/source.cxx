// file      : odb/relational/oracle/source.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <sstream>

#include <odb/relational/source.hxx>

#include <odb/relational/oracle/common.hxx>
#include <odb/relational/oracle/context.hxx>

using namespace std;

namespace relational
{
  namespace oracle
  {
    namespace source
    {
      namespace relational = relational::source;

      struct query_parameters: relational::query_parameters
      {
        query_parameters (base const& x)
            : base (x), i_ (0)
        {
        }

        virtual string
        next ()
        {
          ostringstream ss;
          ss << ":" << ++i_;

          return ss.str ();
        }

      private:
        size_t i_;
      };
      entry<query_parameters> query_parameters_;

      namespace
      {
        const char* string_buffer_types[] =
        {
          "oracle::bind::string",  // CHAR
          "oracle::bind::nstring", // NCHAR
          "oracle::bind::string",  // VARCHAR2
          "oracle::bind::nstring", // NVARCHAR2
          "oracle::bind::raw"      // RAW
        };

        const char* lob_buffer_types[] =
        {
          "oracle::bind::blob",
          "oracle::bind::clob",
          "oracle::bind::nclob"
        };
      }

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
            if (semantics::class_* c = composite (mi.t))
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
        traverse_int32 (member_info& mi)
        {
          os << b << ".type = oracle::bind::" <<
            (unsigned_integer (mi.t) ? "uinteger;" : "integer;")
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".capacity = 4;"
             << b << ".size = 0;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_int64 (member_info& mi)
        {
          os << b << ".type = oracle::bind::" <<
            (unsigned_integer (mi.t) ? "uinteger;" : "integer;")
             << b << ".buffer= &" << arg << "." << mi.var << "value;"
             << b << ".capacity = 8;"
             << b << ".size = 0;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_big_int (member_info& mi)
        {
          os << b << ".type = oracle::bind::number;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = static_cast<ub4> (sizeof (" << arg <<
            "." << mi.var << "value));"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << b << ".type = oracle::bind::binary_float;"
             << b << ".buffer= &" << arg << "." << mi.var << "value;"
             << b << ".capacity = 4;"
             << b << ".size = 0;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_double (member_info& mi)
        {
          os << b << ".type = oracle::bind::binary_double;"
             << b << ".buffer= &" << arg << "." << mi.var << "value;"
             << b << ".capacity = 8;"
             << b << ".size = 0;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_big_float (member_info& mi)
        {
          os << b << ".type = oracle::bind::number;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = static_cast<ub4> (sizeof (" << arg << "." <<
            mi.var << "value));"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << b << ".type = oracle::bind::date;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = static_cast<ub4> (sizeof (" << arg << "." <<
            mi.var << "value));"
             << b << ".size = 0;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_timestamp (member_info& mi)
        {
          os << b << ".type = oracle::bind::timestamp;"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = static_cast<ub4> (sizeof (" << arg << "." <<
            mi.var << "value));"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << b << ".type = " <<
            string_buffer_types[mi.st->type - sql_type::CHAR] << ";"
             << b << ".buffer = " << arg << "." << mi.var << "value;"
             << b << ".capacity = static_cast<ub4> (sizeof (" << arg <<
            "." << mi.var << "value));"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_lob (member_info& mi)
        {
          os << b << ".type = " <<
            lob_buffer_types[mi.st->type - sql_type::BLOB] << ";"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;"
             << b << ".callback = &" << arg << "." << mi.var <<
             "callback;"
             << b << ".context = &" << arg << "." << mi.var << "context;"
             << "if (out)" << endl
             << b << ".buffer = &" << arg << "." << mi.var << "lob;"
             << "else" << endl
             << b << ".size = reinterpret_cast<ub2*> (&" << arg << "." <<
            mi.var << "position_context);"
             << endl;
        }

      private:
        string b;
        string arg;
      };
      entry<bind_member> bind_member_;

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

            traits = "oracle::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (!composite (mi.t))
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            if (object_pointer (member_type (mi.m, key_prefix_)))
            {
              os << "}";

              if (!null (mi.m, key_prefix_))
                os << "else" << endl
                   << "throw null_pointer ();";
            }

            os << "i." << mi.var << "indicator = is_null ? -1 : 0;"
               << "}";
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << traits << "init (" << endl
             << "i." << mi.var << "value," << endl
             << member << ");";
        }

        virtual void
        traverse_int32 (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
        }

        virtual void
        traverse_int64 (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
        }

        virtual void
        traverse_big_int (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
        }

        virtual void
        traverse_double (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
        }

        virtual void
        traverse_big_float (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "sizeof (i." << mi.var << "value)," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");";
        }

        virtual void
        traverse_timestamp (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "sizeof (i." << mi.var << "value)," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << "std::size_t size (0);"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "sizeof (i." << mi.var << "value)," << endl
             << "size," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_lob (member_info& mi)
        {
          os << "i." << mi.var << "position_context = 0;"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "callback.param," << endl
             << "i." << mi.var << "context.param," << endl
             << "is_null," << endl
             << member << ");";
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

          // If this is a wrapped composite value, then we need to
          // "unwrap" it. For simple values this is taken care of
          // by the value_traits specializations.
          //
          if (mi.wrapper != 0 && composite (mi.t))
          {
            // Here we need the wrapper type, not the wrapped type.
            //
            member = "wrapper_traits< " + mi.fq_type (false) + " >::" +
              "set_ref (" + member + ")";
          }

          if (composite (mi.t))
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
                 << "if (i." << mi.var << "indicator == -1)" << endl;

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

            traits = "oracle::value_traits<\n    "
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
        traverse_int32 (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_int64 (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_big_int (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_double (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_big_float (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_timestamp (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_lob (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "callback.result," << endl
             << "i." << mi.var << "context.result," << endl
             << "i." << mi.var << "indicator == -1);"
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
        init_auto_id (semantics::data_member&, string const&)
        {
        }

        virtual void
        init_value_extra ()
        {
          os << "sts.find_statement ().stream_result ();";
        }

        virtual void
        persist_stmt (type& c)
        {
          os << strlit ("INSERT INTO " + table_qname (c) + " (") << endl;

          instance<relational::object_columns> ct (false);
          ct->traverse (c);

          string values;
          instance<relational::persist_statement_params> pt (values);
          pt->traverse (c);

          os << strlit (") VALUES (" + values + ")");

          semantics::data_member* id (id_member (c));

          if (id->count ("auto"))
          {
            ostringstream n;
            n << pt->count () + 1;

            os << endl
               << strlit (" RETURNING " +
                          column_qname (*id) +
                          " INTO :" +
                          n.str ());
          }
        }
      };
      entry<class_> class_entry_;
    }
  }
}
