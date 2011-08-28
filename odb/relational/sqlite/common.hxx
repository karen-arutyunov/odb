// file      : odb/relational/sqlite/common.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SQLITE_COMMON_HXX
#define ODB_RELATIONAL_SQLITE_COMMON_HXX

#include <odb/relational/common.hxx>
#include <odb/relational/sqlite/context.hxx>

namespace relational
{
  namespace sqlite
  {
    struct member_base: virtual relational::member_base, context
    {
      member_base (base const& x): base (x) {}

      // This c-tor is for the direct use inside the sqlite namespace.
      // If you do use this c-tor, you should also explicitly call
      // relational::member_base.
      //
      member_base () {}

      virtual void
      traverse (semantics::data_member& m);

      struct member_info
      {
        semantics::data_member& m; // Member.
        semantics::type& t;        // Member C++ type (m.type () may != t).
        semantics::type* wrapper;  // Wrapper type if member is a wrapper.
                                   // In this case t is the wrapped type.
        sql_type const* st;        // Member SQL type (only simple values).
        string& var;               // Member variable name with trailing '_'.

        // C++ type fq-name.
        //
        string
        fq_type (bool unwrap = true) const
        {
          if (wrapper != 0 && unwrap)
          {
            // Use the hint from the wrapper.
            //
            return t.fq_name (
              wrapper->get<semantics::names*> ("wrapper-hint"));
          }

          // Use the original type from 'm' instead of 't' since the hint
          // may be invalid for a different type. Plus, if a type is
          // overriden, then the fq_type must be as well.
          //
          return fq_type_.empty ()
            ? m.type ().fq_name (m.belongs ().hint ())
            : fq_type_;
        }

        string const& fq_type_;

        member_info (semantics::data_member& m_,
                     semantics::type& t_,
                     semantics::type* wrapper_,
                     string& var_,
                     string const& fq_type)
            : m (m_),
              t (t_),
              wrapper (wrapper_),
              st (0),
              var (var_),
              fq_type_ (fq_type)
        {
        }
      };

      // The false return value indicates that no further callbacks
      // should be called for this member.
      //
      virtual bool
      pre (member_info&)
      {
        return true;
      }

      virtual void
      post (member_info&)
      {
      }

      virtual void
      traverse_composite (member_info&)
      {
      }

      virtual void
      traverse_container (member_info&)
      {
      }

      virtual void
      traverse_object_pointer (member_info& mi)
      {
        traverse_simple (mi);
      }

      virtual void
      traverse_simple (member_info&);

      virtual void
      traverse_integer (member_info&)
      {
      }

      virtual void
      traverse_real (member_info&)
      {
      }

      virtual void
      traverse_text (member_info& m)
      {
        traverse_string (m);
      }

      virtual void
      traverse_blob (member_info& m)
      {
        traverse_string (m);
      }

      // String covers both text and blob.
      //
      virtual void
      traverse_string (member_info&)
      {
      }
    };

    struct member_image_type: member_base
    {
      member_image_type (semantics::type* type = 0,
                         string const& fq_type = string (),
                         string const& key_prefix = string ());
      string
      image_type (semantics::data_member&);

      virtual void
      traverse_composite (member_info&);

      virtual void
      traverse_integer (member_info&);

      virtual void
      traverse_real (member_info&);

      virtual void
      traverse_string (member_info&);

    private:
      string type_;
    };

    struct member_database_type_id: member_base
    {
      member_database_type_id (semantics::type* type = 0,
                               string const& fq_type = string (),
                               string const& key_prefix = string ());
      string
      database_type_id (type&);

      virtual void
      traverse_composite (member_info&);

      virtual void
      traverse_integer (member_info&);

      virtual void
      traverse_real (member_info&);

      virtual void
      traverse_text (member_info&);

      virtual void
      traverse_blob (member_info&);

    private:
      string type_id_;
    };
  }
}
#endif // ODB_RELATIONAL_SQLITE_COMMON_HXX
