// file      : odb/relational/pgsql/common.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>

#include <odb/relational/pgsql/common.hxx>

using namespace std;

namespace relational
{
  namespace pgsql
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
        // Integral types.
        //
      case sql_type::BOOLEAN:
      case sql_type::SMALLINT:
      case sql_type::INTEGER:
      case sql_type::BIGINT:
        {
          traverse_integer (mi);
          break;
        }

        // Float types.
        //
      case sql_type::REAL:
      case sql_type::DOUBLE:
        {
          traverse_float (mi);
          break;
        }
      case sql_type::NUMERIC:
        {
          traverse_numeric (mi);
          break;
        }

        // Data-time types.
        //
      case sql_type::DATE:
      case sql_type::TIME:
      case sql_type::TIMESTAMP:
        {
          traverse_date_time (mi);
          break;
        }

        // String and binary types.
        //
      case sql_type::CHAR:
      case sql_type::VARCHAR:
      case sql_type::TEXT:
      case sql_type::BYTEA:
        {
          traverse_string (mi);
          break;
        }
      case sql_type::BIT:
        {
          traverse_bit (mi);
          break;
        }
      case sql_type::VARBIT:
        {
          traverse_varbit (mi);
          break;
        }
        // Other types.
        //
      case sql_type::UUID:
        {
          traverse_uuid (mi);
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

    namespace
    {
      const char* integer_types[] =
      {
        "bool",
        "short",
        "int",
        "long long"
      };

      const char* float_types[] =
      {
        "float",
        "double"
      };

      const char* date_time_types[] =
      {
        "int",
        "long long",
        "long long"
      };
    }

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
    traverse_integer (member_info& mi)
    {
      type_ += integer_types[mi.st->type - sql_type::BOOLEAN];
    }

    void member_image_type::
    traverse_float (member_info& mi)
    {
      type_ = float_types[mi.st->type - sql_type::REAL];
    }

    void member_image_type::
    traverse_numeric (member_info&)
    {
      type_ = "details::buffer";
    }

    void member_image_type::
    traverse_date_time (member_info& mi)
    {
      type_ = date_time_types[mi.st->type - sql_type::DATE];
    }

    void member_image_type::
    traverse_string (member_info&)
    {
      type_ = "details::buffer";
    }

    void member_image_type::
    traverse_bit (member_info&)
    {
      type_ = "unsigned char*";
    }

    void member_image_type::
    traverse_varbit (member_info&)
    {
      type_ = "details::ubuffer";
    }

    void member_image_type::
    traverse_uuid (member_info&)
    {
      type_ = "unsigned char*";
    }

    //
    // member_database_type
    //

    namespace
    {
      const char* integer_database_id[] =
      {
        "id_boolean",
        "id_smallint",
        "id_integer",
        "id_bigint"
      };

      const char* float_database_id[] =
      {
        "id_real",
        "id_double"
      };

      const char* date_time_database_id[] =
      {
        "id_date",
        "id_time",
        "id_timestamp"
      };

      const char* char_bin_database_id[] =
      {
        "id_string",  // CHAR
        "id_string",  // VARCHAR
        "id_string",  // TEXT,
        "id_bytea"    // BYTEA
      };
   }

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
    traverse_integer (member_info& mi)
    {
      type_id_ = string ("pgsql::") +
        integer_database_id[mi.st->type - sql_type::BOOLEAN];
    }

    void member_database_type_id::
    traverse_float (member_info& mi)
    {
      type_id_ = string ("pgsql::") +
        float_database_id[mi.st->type - sql_type::REAL];
    }

    void member_database_type_id::
    traverse_numeric (member_info&)
    {
      type_id_ = "pgsql::id_numeric";
    }

    void member_database_type_id::
    traverse_date_time (member_info& mi)
    {
      type_id_ = string ("pgsql::") +
        date_time_database_id[mi.st->type - sql_type::DATE];
    }

    void member_database_type_id::
    traverse_string (member_info& mi)
    {
      type_id_ = string ("pgsql::") +
        char_bin_database_id[mi.st->type - sql_type::CHAR];
    }

    void member_database_type_id::
    traverse_bit (member_info&)
    {
      type_id_ = "pgsql::id_bit";
    }

    void member_database_type_id::
    traverse_varbit (member_info&)
    {
      type_id_ = "pgsql::id_varbit";
    }

    void member_database_type_id::
    traverse_uuid (member_info&)
    {
      type_id_ = "pgsql::id_uuid";
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
