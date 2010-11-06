// file      : odb/mysql/common.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cassert>

#include <odb/mysql/common.hxx>

using namespace std;

namespace mysql
{
  //
  // member_base
  //

  void member_base::
  traverse (semantics::data_member& m)
  {
    if (m.count ("transient"))
      return;

    string var;

    if (!var_override_.empty ())
      var = var_override_;
    else
    {
      string const& name (m.name ());
      var = name + (name[name.size () - 1] == '_' ? "" : "_");
    }

    semantics::type& t (type_override_ != 0 ? *type_override_ : m.type ());

    member_info mi (m, t, var, fq_type_override_);

    if (comp_value (t))
    {
      pre (mi);
      traverse_composite (mi);
    }
    else if (container (t))
    {
      pre (mi);
      traverse_container (mi);
    }
    else
    {
      sql_type const& st (db_type (m, key_prefix_));
      mi.st = &st;
      pre (mi);

      switch (st.type)
      {
        // Integral types.
        //
      case sql_type::TINYINT:
      case sql_type::SMALLINT:
      case sql_type::MEDIUMINT:
      case sql_type::INT:
      case sql_type::BIGINT:
        {
          traverse_integer (mi);
          break;
        }

        // Float types.
        //
      case sql_type::FLOAT:
      case sql_type::DOUBLE:
        {
          traverse_float (mi);
          break;
        }
      case sql_type::DECIMAL:
        {
          traverse_decimal (mi);
          break;
        }

        // Data-time types.
        //
      case sql_type::DATE:
      case sql_type::TIME:
      case sql_type::DATETIME:
      case sql_type::TIMESTAMP:
      case sql_type::YEAR:
        {
          traverse_date_time (mi);
          break;
        }

        // String and binary types.
        //
      case sql_type::CHAR:
      case sql_type::VARCHAR:
      case sql_type::TINYTEXT:
      case sql_type::TEXT:
      case sql_type::MEDIUMTEXT:
      case sql_type::LONGTEXT:
        {
          // For string types the limit is in characters rather
          // than in bytes. The fixed-length pre-allocated buffer
          // optimization can only be used for 1-byte encodings.
          // To support this we will need the character encoding
          // in sql_type.
          //
          traverse_long_string (mi);
          break;
        }
      case sql_type::BINARY:
      case sql_type::TINYBLOB:
        {
          // BINARY's range is always 255 or less from MySQL 5.0.3.
          // TINYBLOB can only store up to 255 bytes.
          //
          traverse_short_string (mi);
          break;
        }
      case sql_type::VARBINARY:
      case sql_type::BLOB:
      case sql_type::MEDIUMBLOB:
      case sql_type::LONGBLOB:
        {
          if (st.range && st.range_value <= 255)
            traverse_short_string (mi);
          else
            traverse_long_string (mi);

          break;
        }

        // Other types.
        //
      case sql_type::BIT:
        {
          traverse_bit (mi);
          break;
        }
      case sql_type::ENUM:
        {
          traverse_enum (mi);
          break;
        }
      case sql_type::SET:
        {
          traverse_set (mi);
          break;
        }
      case sql_type::invalid:
        {
          assert (false);
          break;
        }
      }
    }

    post (mi);
  }

  //
  // member_image_type
  //

  namespace
  {
    const char* integer_types[] =
    {
      "char",
      "short",
      "int",
      "int",
      "long long"
    };

    const char* float_types[] =
    {
      "float",
      "double"
    };
  }

  member_image_type::
  member_image_type (context& c)
      : member_base (c)
  {
  }

  member_image_type::
  member_image_type (context& c,
                     semantics::type& type,
                     string const& fq_type,
                     string const& key_prefix)
      : member_base (c, "", type, fq_type, key_prefix)
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
    if (mi.st->unsign)
      type_ = "unsigned ";
    else if (mi.st->type == sql_type::TINYINT)
      type_ = "signed ";

    type_ += integer_types[mi.st->type - sql_type::TINYINT];
  }

  void member_image_type::
  traverse_float (member_info& mi)
  {
    type_ = float_types[mi.st->type - sql_type::FLOAT];
  }

  void member_image_type::
  traverse_decimal (member_info&)
  {
    type_ = "details::buffer";
  }

  void member_image_type::
  traverse_date_time (member_info& mi)
  {
    if (mi.st->type == sql_type::YEAR)
      type_ = "short";
    else
      type_ = "MYSQL_TIME";
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
  traverse_enum (member_info&)
  {
    // Represented as string.
    //
    type_ = "details::buffer";
  }

  void member_image_type::
  traverse_set (member_info&)
  {
    // Represented as string.
    //
    type_ = "details::buffer";
  }

  //
  // member_database_type
  //

  namespace
  {
    const char* integer_database_id[] =
    {
      "id_tiny",
      "id_utiny",
      "id_short",
      "id_ushort",
      "id_long",   // INT24
      "id_ulong",  // INT24 UNSIGNED
      "id_long",
      "id_ulong",
      "id_longlong",
      "id_ulonglong"
    };

    const char* float_database_id[] =
    {
      "id_float",
      "id_double"
    };

    const char* date_time_database_id[] =
    {
      "id_date",
      "id_time",
      "id_datetime",
      "id_timestamp",
      "id_year"
    };

    const char* char_bin_database_id[] =
    {
      "id_string", // CHAR
      "id_blob",   // BINARY,
      "id_string", // VARCHAR
      "id_blob",   // VARBINARY
      "id_string", // TINYTEXT
      "id_blob",   // TINYBLOB
      "id_string", // TEXT
      "id_blob",   // BLOB
      "id_string", // MEDIUMTEXT
      "id_blob",   // MEDIUMBLOB
      "id_string", // LONGTEXT
      "id_blob"    // LONGBLOB
    };
  }

  member_database_type::
  member_database_type (context& c)
      : member_base (c)
  {
  }

  member_database_type::
  member_database_type (context& c,
                        semantics::type& type,
                        string const& fq_type,
                        string const& key_prefix)
      : member_base (c, "", type, fq_type, key_prefix)
  {
  }

  string member_database_type::
  database_type (type& m)
  {
    type_.clear ();
    member_base::traverse (m);
    return type_;
  }

  void member_database_type::
  traverse_composite (member_info&)
  {
    assert (false);
  }

  void member_database_type::
  traverse_integer (member_info& mi)
  {
    size_t i ((mi.st->type - sql_type::TINYINT) * 2 + (mi.st->unsign ? 1 : 0));
    type_ = string ("mysql::") + integer_database_id[i];
  }

  void member_database_type::
  traverse_float (member_info& mi)
  {
    type_ = string ("mysql::") +
      float_database_id[mi.st->type - sql_type::FLOAT];
  }

  void member_database_type::
  traverse_decimal (member_info&)
  {
    type_ = "mysql::id_decimal";
  }

  void member_database_type::
  traverse_date_time (member_info& mi)
  {
    type_ = string ("mysql::") +
      date_time_database_id[mi.st->type - sql_type::DATE];
  }

  void member_database_type::
  traverse_string (member_info& mi)
  {
    type_ = string ("mysql::") +
      char_bin_database_id[mi.st->type - sql_type::CHAR];
  }

  void member_database_type::
  traverse_bit (member_info&)
  {
    type_ = "mysql::id_bit";
  }

  void member_database_type::
  traverse_enum (member_info&)
  {
    type_ = "mysql::id_enum";
  }

  void member_database_type::
  traverse_set (member_info&)
  {
    type_ = "mysql::id_set";
  }

  //
  // query_columns
  //

  query_columns::
  query_columns (context& c)
      : object_columns_base (c),
        context (c),
        decl_ (true),
        member_image_type_ (c),
        member_database_type_ (c)
  {
  }

  query_columns::
  query_columns (context& c, semantics::class_& cl)
      : object_columns_base (c),
        context (c),
        decl_ (false),
        member_image_type_ (c),
        member_database_type_ (c)
  {
    scope_ = "access::object_traits< " + cl.fq_name () + " >::query_type";
    table_ = table_name (cl);
  }

  void query_columns::
  composite (semantics::data_member& m, semantics::type& t)
  {
    string name (public_name (m));

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "struct " << name
         << "{";

      object_columns_base::composite (m, t);

      os << "};";
    }
    else
    {
      string old_scope (scope_);
      scope_ += "::" + name;

      object_columns_base::composite (m, t);

      scope_ = old_scope;
    }
  }

  void query_columns::
  column (semantics::data_member& m, string const& col_name, bool)
  {
    string name (public_name (m));
    string db_type (member_database_type_.database_type (m));

    string type (
      "mysql::value_traits< "
      + m.type ().fq_name (m.belongs ().hint ()) + ", "
      + member_image_type_.image_type (m) + ", "
      + db_type
      + " >::query_type");

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "static const mysql::query_column<" << endl
         << "  " << type << "," << endl
         << "  " << db_type << ">" << endl
         << name << ";"
         << endl;
    }
    else
    {
      string column ("\"`" + table_ + "`.`" + col_name + "`\"");

      os << "const mysql::query_column<" << endl
         << "  " << type << "," << endl
         << "  " << db_type << ">" << endl
         << scope_ << "::" << name << " (" << endl
         << column << ");"
         << endl;
    }
  }
}
