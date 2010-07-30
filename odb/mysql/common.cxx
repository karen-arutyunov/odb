// file      : odb/mysql/common.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

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
      {
        // BINARY's range is always 255 or less from MySQL 5.0.3.
        //
        traverse_short_string (m, t);
        break;
      }
    case sql_type::VARBINARY:
    case sql_type::TINYBLOB:
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
}
