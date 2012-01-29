// file      : odb/relational/sqlite/common.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>

#include <odb/relational/sqlite/common.hxx>

using namespace std;

namespace relational
{
  namespace sqlite
  {
    //
    // member_base
    //

    void member_base::
    traverse (semantics::data_member& m)
    {
      if (transient (m))
        return;

      string var;

      if (!var_override_.empty ())
        var = var_override_;
      else
      {
        string const& name (m.name ());
        var = name + (name[name.size () - 1] == '_' ? "" : "_");
      }

      bool cq (type_override_ != 0 ? false : const_type (m.type ()));
      semantics::type& t (type_override_ != 0 ? *type_override_ : utype (m));

      semantics::type* cont;
      if (semantics::class_* c = composite_wrapper (t))
      {
        // If t is a wrapper, pass the wrapped type. Also pass the
        // original, wrapper type.
        //
        member_info mi (m,
                        *c,
                        (wrapper (t) ? &t : 0),
                        cq,
                        var,
                        fq_type_override_);
        if (pre (mi))
        {
          traverse_composite (mi);
          post (mi);
        }
      }
      // This cannot be a container if we have a type override.
      //
      else if (type_override_ == 0 && (cont = context::container (m)))
      {
        // The same unwrapping logic as for composite values.
        //
        member_info mi (m,
                        *cont,
                        (wrapper (t) ? &t : 0),
                        cq,
                        var,
                        fq_type_override_);
        if (pre (mi))
        {
          traverse_container (mi);
          post (mi);
        }
      }
      else
      {
        sql_type const& st (column_sql_type (m, key_prefix_));

        if (semantics::class_* c = object_pointer (t))
        {
          member_info mi (m,
                          utype (*id_member (*c)),
                          0,
                          cq,
                          var,
                          fq_type_override_);
          mi.st = &st;
          if (pre (mi))
          {
            traverse_object_pointer (mi);
            post (mi);
          }
        }
        else
        {
          member_info mi (m, t, 0, cq, var, fq_type_override_);
          mi.st = &st;
          if (pre (mi))
          {
            traverse_simple (mi);
            post (mi);
          }
        }
      }
    }

    void member_base::
    traverse_simple (member_info& mi)
    {
      switch (mi.st->type)
      {
      case sql_type::INTEGER:
        {
          traverse_integer (mi);
          break;
        }
      case sql_type::REAL:
        {
          traverse_real (mi);
          break;
        }
      case sql_type::TEXT:
        {
          traverse_text (mi);
          break;
        }
      case sql_type::BLOB:
        {
          traverse_blob (mi);
          break;
        }
      case sql_type::invalid:
        {
          assert (false);
          break;
        }
      }
    }

    //
    // member_image_type
    //

    member_image_type::
    member_image_type (semantics::type* type,
                       string const& fq_type,
                       string const& key_prefix)
        : relational::member_base (type, fq_type, key_prefix)
    {
    }

    string member_image_type::
    image_type (semantics::data_member& m)
    {
      type_.clear ();
      member_base::traverse (m);
      return type_;
    }

    void member_image_type::
    traverse_composite (member_info& mi)
    {
      type_ = "composite_value_traits< " + mi.fq_type () + " >::image_type";
    }

    void member_image_type::
    traverse_integer (member_info&)
    {
      type_ = "long long";
    }

    void member_image_type::
    traverse_real (member_info&)
    {
      type_ = "double";
    }

    void member_image_type::
    traverse_string (member_info&)
    {
      type_ = "details::buffer";
    }

    //
    // member_database_type
    //

    member_database_type_id::
    member_database_type_id (semantics::type* type,
                          string const& fq_type,
                          string const& key_prefix)
        : relational::member_base (type, fq_type, key_prefix)
    {
    }

    string member_database_type_id::
    database_type_id (type& m)
    {
      type_id_.clear ();
      member_base::traverse (m);
      return type_id_;
    }

    void member_database_type_id::
    traverse_composite (member_info&)
    {
      assert (false);
    }

    void member_database_type_id::
    traverse_integer (member_info&)
    {
      type_id_ = "sqlite::id_integer";
    }

    void member_database_type_id::
    traverse_real (member_info&)
    {
      type_id_ = "sqlite::id_real";
    }

    void member_database_type_id::
    traverse_text (member_info&)
    {
      type_id_ = "sqlite::id_text";
    }

    void member_database_type_id::
    traverse_blob (member_info&)
    {
      type_id_ = "sqlite::id_blob";
    }

    //
    // query_columns
    //

    struct query_columns: relational::query_columns, context
    {
      query_columns (base const& x): base (x) {}

      virtual string
      database_type_id (semantics::data_member& m)
      {
        return member_database_type_id_.database_type_id (m);
      }

    private:
      member_database_type_id member_database_type_id_;
    };
    entry<query_columns> query_columns_;
  }
}
