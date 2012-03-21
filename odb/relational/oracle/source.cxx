// file      : odb/relational/oracle/source.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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

      struct bind_member: relational::bind_member_impl<sql_type>,
                          member_base
      {
        bind_member (base const& x)
            : member_base::base (x),      // virtual base
              member_base::base_impl (x), // virtual base
              base_impl (x),
              member_base (x)
        {
        }

        virtual void
        traverse_int32 (member_info& mi)
        {
          os << b << ".type = oracle::bind::" <<
            (unsigned_integer (mi.t) ? "uinteger" : "integer") << ";"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".capacity = 4;"
             << b << ".size = 0;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_int64 (member_info& mi)
        {
          os << b << ".type = oracle::bind::" <<
            (unsigned_integer (mi.t) ? "uinteger" : "integer") << ";"
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
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_interval_ym (member_info& mi)
        {
          os << b << ".type = oracle::bind::interval_ym;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;";
        }

        virtual void
        traverse_interval_ds (member_info& mi)
        {
          os << b << ".type = oracle::bind::interval_ds;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
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
             << b << ".buffer = &" << arg << "." << mi.var << "lob;"
             << b << ".indicator = &" << arg << "." << mi.var << "indicator;"
             << b << ".callback = &" << arg << "." << mi.var << "callback;"
             << endl;
        }
      };
      entry<bind_member> bind_member_;

      //
      // init image
      //

      struct init_image_member: relational::init_image_member_impl<sql_type>,
                                member_base
      {
        init_image_member (base const& x)
            : member_base::base (x),      // virtual base
              member_base::base_impl (x), // virtual base
              base_impl (x),
              member_base (x)
        {
        }

        virtual void
        set_null (member_info& mi)
        {
          os << "i." << mi.var << "indicator = -1;";
        }

        virtual void
        traverse_int32 (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
        }

        virtual void
        traverse_int64 (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
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
             << "i." << mi.var << "indicator = is_null ? -1 : 0;"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
        }

        virtual void
        traverse_double (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
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
             << "i." << mi.var << "indicator = is_null ? -1 : 0;"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_date (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
        }

        virtual void
        traverse_timestamp (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
        }

        virtual void
        traverse_interval_ym (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
        }

        virtual void
        traverse_interval_ds (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value, is_null, " << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
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
             << "i." << mi.var << "indicator = is_null ? -1 : 0;"
             << "i." << mi.var << "size = static_cast<ub2> (size);";
        }

        virtual void
        traverse_lob (member_info& mi)
        {
          os << "i." << mi.var << "lob.position = 0;"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "callback.callback.param," << endl
             << "i." << mi.var << "callback.context.param," << endl
             << "is_null," << endl
             << member << ");"
             << "i." << mi.var << "indicator = is_null ? -1 : 0;";
        }
      };
      entry<init_image_member> init_image_member_;

      //
      // init value
      //

      struct init_value_member: relational::init_value_member_impl<sql_type>,
                                member_base
      {
        init_value_member (base const& x)
            : member_base::base (x),      // virtual base
              member_base::base_impl (x), // virtual base
              base_impl (x),
              member_base (x)
        {
        }

        virtual void
        get_null (member_info& mi)
        {
          os << "i." << mi.var << "indicator == -1";
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
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_interval_ym (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }

        virtual void
        traverse_interval_ds (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
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
             << "i." << mi.var << "callback.callback.result," << endl
             << "i." << mi.var << "callback.context.result," << endl
             << "i." << mi.var << "indicator == -1);"
             << endl;
        }
      };
      entry<init_value_member> init_value_member_;

      struct container_traits: relational::container_traits, context
      {
        container_traits (base const& x): base (x) {}

        virtual void
        cache_result (string const&)
        {
          // Caching is not necessary since Oracle can execute several
          // interleaving statements.
          //
        }

        virtual void
        init_value_extra ()
        {
          os << "sts.select_all_statement ().stream_result ();"
             << endl;
        }
      };
      entry<container_traits> container_traits_;

      struct class_: relational::class_, context
      {
        class_ (base const& x): base (x) {}

        virtual void
        init_auto_id (semantics::data_member&, string const&)
        {
        }

        virtual void
        init_image_pre (type& c)
        {
          if (options.generate_query () &&
              !(composite (c) || (abstract (c) && !polymorphic (c))))
          {
            type* poly_root (polymorphic (c));
            bool poly_derived (poly_root != 0 && poly_root != &c);

            if (poly_derived)
              os << "{"
                 << "root_traits::image_type& ri (root_image (i));"
                 << endl;

            string i (poly_derived ? "ri" : "i");

            os << "if (" << i << ".change_callback_.callback != 0)" << endl
               << "(" << i << ".change_callback_.callback) (" <<
              i << ".change_callback_.context, 0);";

            if (poly_derived)
              os << "}";
            else
              os << endl;
          }
        }

        virtual void
        init_value_extra ()
        {
          os << "st.stream_result ();";
        }

        virtual void
        persist_statement_extra (type& c,
                                 relational::query_parameters& qp,
                                 persist_position p)
        {
          if (p != persist_after_values)
            return;

          semantics::data_member* id (id_member (c));

          if (id != 0 && id->count ("auto"))
          {
            os << endl
               << strlit (" RETURNING " +
                          column_qname (*id) +
                          " INTO " +
                          qp.next ());
          }
        }
      };
      entry<class_> class_entry_;
    }
  }
}
