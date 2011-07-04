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

      struct class_: relational::class_
      {
        class_ (base const& x): base (x) {}

        virtual void
        object_public_extra_post (type& t)
        {
          if (abstract (t))
            return;

          // Statement names.
          //
          os << "static const char* const persist_statement_name;"
             << "static const char* const find_statement_name;"
             << "static const char* const update_statement_name;"
             << "static const char* const erase_statement_name;"
             << endl;

          // Statement types.
          //
          os << "static const unsigned int persist_statement_types[];"
             << "static const unsigned int find_statement_types[];"
             << "static const unsigned int update_statement_types[];"
             << "static const unsigned int erase_statement_types[];"
             << endl;

          // Query statement name.
          //
          os << "static const char* const query_statement_name;"
             << endl;
        }
      };
      entry<class_> class_entry_;

      struct container_traits: relational::container_traits, context
      {
        container_traits (base const& x): base (x) {}

        virtual void
        container_public_extra_pre (semantics::data_member&)
        {
          if (!c_.count ("object") || abstract (c_))
            return;

          // Container statement names.
          //
          os << "static const char* const select_all_name;"
             << "static const char* const insert_one_name;"
             << "static const char* const delete_all_name;"
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
          unsigned int n (mi.st->range / 8 + (mi.st->range % 8 ? 1 : 0));

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
