// file      : odb/mysql/common.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/mysql/common.hxx>

using namespace std;

namespace mysql
{
  void member_base::
  traverse (type& m)
  {
    if (m.count ("transient") || id_ && !m.count ("id"))
      return;

    if (id_)
      var = "id_";
    else
    {
      string const& name (m.name ());
      var = name + (name[name.size () - 1] == '_' ? "" : "_");
    }

    pre (m);

    sql_type const& t (db_type (m));

    switch (t.type)
    {
      // Integral types.
      //
    case sql_type::TINYINT:
    case sql_type::SMALLINT:
    case sql_type::MEDIUMINT:
    case sql_type::INT:
    case sql_type::BIGINT:
      {
        traverse_integer (m, t);
        break;
      }

      // Float types.
      //
    case sql_type::FLOAT:
    case sql_type::DOUBLE:
      {
        traverse_float (m, t);
        break;
      }
    case sql_type::DECIMAL:
      {
        traverse_decimal (m, t);
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
        traverse_date_time (m, t);
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
        traverse_long_string (m, t);
        break;
      }
    case sql_type::BINARY:
    case sql_type::TINYBLOB:
      {
        // BINARY's range is always 255 or less from MySQL 5.0.3.
        // TINYBLOB can only store up to 255 bytes.
        //
        traverse_short_string (m, t);
        break;
      }
    case sql_type::VARBINARY:
    case sql_type::BLOB:
    case sql_type::MEDIUMBLOB:
    case sql_type::LONGBLOB:
      {
        if (t.range && t.range_value <= 255)
          traverse_short_string (m, t);
        else
          traverse_long_string (m, t);

        break;
      }

      // Other types.
      //
    case sql_type::BIT:
      {
        traverse_bit (m, t);
        break;
      }
    case sql_type::ENUM:
      {
        traverse_enum (m, t);
        break;
      }
    case sql_type::SET:
      {
        traverse_set (m, t);
        break;
      }
    }

    post (m);
  }

  //
  // query_column
  //

  namespace
  {
    const char* integer_image_id[] =
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

    const char* float_image_id[] =
    {
      "id_float",
      "id_double"
    };

    const char* date_time_image_id[] =
    {
      "id_date",
      "id_time",
      "id_datetime",
      "id_timestamp",
      "id_year"
    };

    const char* char_bin_image_id[] =
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

  query_column::
  query_column (context& c)
      : member_base (c, false), decl_ (true)
  {
  }

  query_column::
  query_column (context& c, semantics::class_& cl)
      : member_base (c, false), decl_ (false)
  {
    scope_ = "access::object_traits< " + cl.fq_name () + " >::query_type";
    table_ = table_name (cl);
  }

  void query_column::
  pre (type& m)
  {
    type_ = "mysql::value_traits< "
      + m.type ().fq_name (m.belongs ().hint ())
      + " >::value_type";

    name_ = escape (public_name (m));

    if (decl_)
      os << "// " << name_ << endl
         << "//" << endl;
    else
      column_ = "\"`" + table_ + "`.`" + column_name (m) + "`\"";
  }

  void query_column::
  traverse_integer (type& m, sql_type const& t)
  {
    size_t i ((t.type - sql_type::TINYINT) * 2 + (t.unsign ? 1 : 0));

    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << integer_image_id[i] << ">" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << integer_image_id[i] << ">" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_float (type& m, sql_type const& t)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << float_image_id[t.type - sql_type::FLOAT] << ">" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << float_image_id[t.type - sql_type::FLOAT] << ">" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_decimal (type& m, sql_type const& t)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_string>" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_string>" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_date_time (type& m, sql_type const& t)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << date_time_image_id[t.type - sql_type::DATE] << ">" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << date_time_image_id[t.type - sql_type::DATE] << ">" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_short_string (type& m, sql_type const& t)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << char_bin_image_id[t.type - sql_type::CHAR] << ">" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << char_bin_image_id[t.type - sql_type::CHAR] << ">" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_long_string (type& m, sql_type const& t)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << char_bin_image_id[t.type - sql_type::CHAR] << ">" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::" << char_bin_image_id[t.type - sql_type::CHAR] << ">" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_bit (type& m, sql_type const& t)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_blob>" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_blob>" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_enum (type& m, sql_type const&)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_string>" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_string>" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }

  void query_column::
  traverse_set (type& m, sql_type const&)
  {
    if (decl_)
    {
      os << "static const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_string>" << endl
         << name_ << ";"
         << endl;
    }
    else
    {
      os << "const mysql::query_column<" << endl
         << "  " << type_ << "," << endl
         << "  mysql::id_string>" << endl
         << scope_ << "::" << name_ << " (" << endl
         << column_ << ");"
         << endl;
    }
  }
}
