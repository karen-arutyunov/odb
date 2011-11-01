// file      : odb/relational/pgsql/header.cxx
// author    : Constantin Michael <constantin@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/header.hxx>

#include <odb/relational/pgsql/common.hxx>
#include <odb/relational/pgsql/context.hxx>

namespace relational
{
  namespace pgsql
  {
    namespace header
    {
      namespace relational = relational::header;

      struct class1: relational::class1
      {
        class1 (base const& x): base (x) {}

        virtual void
        object_public_extra_post (type& c)
        {
          if (abstract (c))
            return;

          semantics::data_member* id (id_member (c));
          semantics::data_member* optimistic (context::optimistic (c));

          column_count_type const& cc (column_count (c));

          // Statement names.
          //
          os << "static const char persist_statement_name[];";

          if (id != 0)
          {
            os << "static const char find_statement_name[];";

            if (cc.total != cc.id + cc.inverse + cc.readonly)
              os << "static const char update_statement_name[];";

            os << "static const char erase_statement_name[];";

            if (optimistic != 0)
              os << "static const char optimistic_erase_statement_name[];";
          }

          // Query statement name.
          //
          if (options.generate_query ())
            os << "static const char query_statement_name[];"
               << "static const char erase_query_statement_name[];";

          os << endl;

          // Statement types.
          //
          os << "static const unsigned int persist_statement_types[];";

          if (id != 0)
          {
            os << "static const unsigned int find_statement_types[];";

            if (cc.total != cc.id + cc.inverse + cc.readonly)
              os << "static const unsigned int update_statement_types[];";

            os << "static const unsigned int erase_statement_types[];";

            if (optimistic != 0)
              os << "static const unsigned int " <<
                "optimistic_erase_statement_types[];";
          }

          os << endl;
        }

        virtual void
        view_public_extra_post (type&)
        {
          // Statement names.
          //
          os << "static const char query_statement_name[];"
             << endl;
        }
      };
      entry<class1> class1_entry_;

      struct container_traits: relational::container_traits, context
      {
        container_traits (base const& x): base (x) {}

        virtual void
        container_public_extra_pre (semantics::data_member&)
        {
          if (!object (c_) || abstract (c_))
            return;

          // Container statement names.
          //
          os << "static const char select_all_name[];"
             << "static const char insert_one_name[];"
             << "static const char delete_all_name[];"
             << endl;

          // Container statement types.
          //
          os << "static const unsigned int select_all_types[];"
             << "static const unsigned int insert_one_types[];"
             << "static const unsigned int delete_all_types[];"
             << endl;
        }
      };
      entry<container_traits> container_traits_;

      struct image_member: relational::image_member, member_base
      {
        image_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x),
              member_image_type_ (base::type_override_,
                                  base::fq_type_override_,
                                  base::key_prefix_)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi.t))
            return false;

          image_type = member_image_type_.image_type (mi.m);

          if (var_override_.empty ())
            os << "// " << mi.m.name () << endl
               << "//" << endl;

          return true;
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << image_type << " " << mi.var << "value;"
             << endl;
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << image_type << " " << mi.var << "value;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_float (member_info& mi)
        {
          os << image_type << " " << mi.var << "value;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_numeric (member_info& mi)
        {
          // Exchanged as strings. Can have up to 1000 digits not counting
          // '-' and '.'.
          //

          os << image_type << " " << mi.var << "value;"
             << "std::size_t " << mi.var << "size;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_date_time (member_info& mi)
        {
          os << image_type << " " << mi.var << "value;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << image_type << " " << mi.var << "value;"
             << "std::size_t " << mi.var << "size;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_bit (member_info& mi)
        {
          // Additional 4 bytes at the beginning of the array specify
          // the number of significant bits in the image. This number
          // is stored in network byte order.
          //
          unsigned int n (4 + mi.st->range / 8 + (mi.st->range % 8 ? 1 : 0));

          os << "unsigned char " << mi.var << "value[" << n << "];"
             << "std::size_t " << mi.var << "size;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_varbit (member_info& mi)
        {
          os << image_type << " " << mi.var << "value;"
             << "std::size_t " << mi.var << "size;"
             << "bool " << mi.var << "null;"
             << endl;
        }

        virtual void
        traverse_uuid (member_info& mi)
        {
          // UUID is a 16-byte sequence.
          //
          os << "unsigned char " << mi.var << "value[16];"
             << "bool " << mi.var << "null;"
             << endl;
        }

      private:
        string image_type;

        member_image_type member_image_type_;
      };
      entry<image_member> image_member_;
    }
  }
}
