// file      : odb/mysql/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <cstddef> // std::size_t
#include <sstream>

#include <odb/mysql/common.hxx>
#include <odb/mysql/source.hxx>

using namespace std;

namespace mysql
{
  namespace
  {
    struct member_column: traversal::data_member, context
    {
      member_column (context& c, string const& suffix = "")
          : context (c), suffix_ (suffix), first_ (true)
      {
      }

      virtual void
      traverse (type& m)
      {
        if (m.count ("transient"))
          return;

        if (first_)
          first_ = false;
        else
          os << ",\"" << endl;

        os << "\"`" << column_name (m) << "`" << suffix_;
      }

    private:
      string suffix_;
      bool first_;
    };

    const char* integer_buffer_types[] =
    {
      "MYSQL_TYPE_TINY",
      "MYSQL_TYPE_SHORT",
      "MYSQL_TYPE_LONG",     // *_bind_param() doesn't support INT24.
      "MYSQL_TYPE_LONG",
      "MYSQL_TYPE_LONGLONG"
    };

    const char* float_buffer_types[] =
    {
      "MYSQL_TYPE_FLOAT",
      "MYSQL_TYPE_DOUBLE"
    };

    const char* date_time_buffer_types[] =
    {
      "MYSQL_TYPE_DATE",
      "MYSQL_TYPE_TIME",
      "MYSQL_TYPE_DATETIME",
      "MYSQL_TYPE_TIMESTAMP",
      "MYSQL_TYPE_SHORT"
    };

    const char* char_bin_buffer_types[] =
    {
      "MYSQL_TYPE_STRING", // CHAR
      "MYSQL_TYPE_BLOB",   // BINARY,
      "MYSQL_TYPE_STRING", // VARCHAR
      "MYSQL_TYPE_BLOB",   // VARBINARY
      "MYSQL_TYPE_STRING", // TINYTEXT
      "MYSQL_TYPE_BLOB",   // TINYBLOB
      "MYSQL_TYPE_STRING", // TEXT
      "MYSQL_TYPE_BLOB",   // BLOB
      "MYSQL_TYPE_STRING", // MEDIUMTEXT
      "MYSQL_TYPE_BLOB",   // MEDIUMBLOB
      "MYSQL_TYPE_STRING", // LONGTEXT
      "MYSQL_TYPE_BLOB"    // LONGBLOB
    };

    struct bind_member: member_base
    {
      bind_member (context& c, bool id)
          : member_base (c, id), index_ (0)
      {
      }

      virtual void
      pre (type& m)
      {
        ostringstream ostr;
        ostr << "b.bind[" << index_ << "UL]";
        b = ostr.str ();

        if (!id_)
          os << "// " << m.name () << endl
             << "//" << endl;
      }

      virtual void
      post (type&)
      {
        index_++;
      }

      virtual void
      traverse_integer (type&, sql_type const& t)
      {
        // While the is_unsigned should indicate whether the
        // buffer variable is unsigned, rather than whether the
        // database type is unsigned, in case of the image types,
        // this is the same.
        //
        os << b << ".buffer_type = " <<
          integer_buffer_types[t.type - sql_type::TINYINT] << ";"
           << b << ".is_unsigned = " << (t.unsign ? "1" : "0") << ";"
           << b << ".buffer = &i." << var << "value;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_float (type&, sql_type const& t)
      {
        os << b << ".buffer_type = " <<
          float_buffer_types[t.type - sql_type::FLOAT] << ";"
           << b << ".buffer = &i." << var << "value;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_decimal (type&, sql_type const& t)
      {
        os << b << ".buffer_type = MYSQL_TYPE_NEWDECIMAL;"
           << b << ".buffer = i." << var << "value;"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "sizeof (i." << var << "value));"
           << b << ".length = &i." << var << "size;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_date_time (type&, sql_type const& t)
      {
        os << b << ".buffer_type = " <<
          date_time_buffer_types[t.type - sql_type::DATE] << ";"
           << b << ".buffer = &i." << var << "value;";

        if (t.type == sql_type::YEAR)
          os << b << ".is_unsigned = 0;";

        os << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_short_string (type&, sql_type const& t)
      {
        // MySQL documentation is quite confusing about the use of
        // buffer_length and length when it comes to input parameters.
        // Source code, however, tells us that it uses buffer_length
        // only if length is NULL.
        //
        os << b << ".buffer_type = " <<
          char_bin_buffer_types[t.type - sql_type::CHAR] << ";"
           << b << ".buffer = i." << var << "value;"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "sizeof (i." << var << "value));"
           << b << ".length = &i." << var << "size;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_long_string (type&, sql_type const& t)
      {
        os << b << ".buffer_type = " <<
          char_bin_buffer_types[t.type - sql_type::CHAR] << ";"
           << b << ".buffer = i." << var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "i." << var << "value.capacity ());"
           << b << ".length = &i." << var << "size;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_bit (type&, sql_type const& t)
      {
        // Treated as a BLOB.
        //
        os << b << ".buffer_type = MYSQL_TYPE_BLOB;"
           << b << ".buffer = i." << var << "value;"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "sizeof (i." << var << "value));"
           << b << ".length = &i." << var << "size;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_enum (type&, sql_type const&)
      {
        // Represented as a string.
        //
        os << b << ".buffer_type = MYSQL_TYPE_STRING;"
           << b << ".buffer = i." << var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "i." << var << "value.capacity ());"
           << b << ".length = &i." << var << "size;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

      virtual void
      traverse_set (type&, sql_type const&)
      {
        // Represented as a string.
        //
        os << b << ".buffer_type = MYSQL_TYPE_STRING;"
           << b << ".buffer = i." << var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "i." << var << "value.capacity ());"
           << b << ".length = &i." << var << "size;"
           << b << ".is_null = &i." << var << "null;"
           << endl;
      }

    private:
      string b;
      size_t index_;
    };

    struct grow_member: member_base
    {
      grow_member (context& c)
          : member_base (c, false), index_ (0)
      {
      }

      virtual void
      pre (type& m)
      {
        ostringstream ostr;
        ostr << "e[" << index_ << "UL]";
        e = ostr.str ();

        os << "// " << m.name () << endl
           << "//" << endl;
      }

      virtual void
      post (type&)
      {
        index_++;
      }

      virtual void
      traverse_integer (type&, sql_type const& t)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_float (type&, sql_type const& t)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_decimal (type&, sql_type const& t)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_date_time (type&, sql_type const& t)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_short_string (type&, sql_type const& t)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_long_string (type&, sql_type const& t)
      {
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << var << "value.capacity (i." << var << "size);"
           << "r = true;"
           << "}";
      }

      virtual void
      traverse_bit (type&, sql_type const& t)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_enum (type&, sql_type const&)
      {
        // Represented as a string.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << var << "value.capacity (i." << var << "size);"
           << "r = true;"
           << "}";
      }

      virtual void
      traverse_set (type&, sql_type const&)
      {
        // Represented as a string.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << var << "value.capacity (i." << var << "size);"
           << "r = true;"
           << "}";
      }

    private:
      string e;
      size_t index_;
    };

    //
    //
    struct init_image_member: member_base
    {
      init_image_member (context& c, bool id)
          : member_base (c, id)
      {
      }

      virtual void
      pre (type& m)
      {
        type = m.type ().fq_name (m.belongs ().hint ());

        if (id_)
          member = "id";
        else
        {
          string const& name (m.name ());
          member = "o." + name;

          os << "// " << name << endl
             << "//" << endl;
        }
      }

      virtual void
      traverse_integer (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value, is_null, " << member << ");"
           << "i." << var << "null = is_null;"
           << endl;
      }

      virtual void
      traverse_float (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value, is_null, " << member << ");"
           << "i." << var << "null = is_null;"
           << endl;
      }

      virtual void
      traverse_decimal (type& m, sql_type const&)
      {
        os << "{"
           << "std::size_t size;"
           << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value," << endl
           << "sizeof (i." << var << "value)," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << var << "size = static_cast<unsigned long> (size);"
           << "i." << var << "null = is_null;"
           << "}";
      }

      virtual void
      traverse_date_time (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value, is_null, " << member << ");"
           << "i." << var << "null = is_null;"
           << endl;
      }

      virtual void
      traverse_short_string (type& m, sql_type const&)
      {
        os << "{"
           << "std::size_t size;"
           << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value," << endl
           << "sizeof (i." << var << "value)," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << var << "size = static_cast<unsigned long> (size);"
           << "i." << var << "null = is_null;"
           << "}";
      }

      virtual void
      traverse_long_string (type& m, sql_type const&)
      {
        os << "{"
           << "std::size_t size;"
           << "std::size_t cap (i." << var << "value.capacity ());"
           << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << var << "size = static_cast<unsigned long> (size);"
           << "i." << var << "null = is_null;"
           << "grew = grew || (cap != i." << var << "value.capacity ());"
           << "}";
      }

      virtual void
      traverse_bit (type& m, sql_type const&)
      {
        // Represented as a BLOB.
        //
        os << "{"
           << "std::size_t size;"
           << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value," << endl
           << "sizeof (i." << var << "value)," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << var << "size = static_cast<unsigned long> (size);"
           << "i." << var << "null = is_null;"
           << "}";
      }

      virtual void
      traverse_enum (type& m, sql_type const&)
      {
        // Represented as a string.
        //
        os << "{"
           << "std::size_t size;"
           << "std::size_t cap (i." << var << "value.capacity ());"
           << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << var << "size = static_cast<unsigned long> (size);"
           << "i." << var << "null = is_null;"
           << "grew = grew || (cap != i." << var << "value.capacity ());"
           << "}";
      }

      virtual void
      traverse_set (type& m, sql_type const&)
      {
        // Represented as a string.
        //
        os << "{"
           << "std::size_t size;"
           << "std::size_t cap (i." << var << "value.capacity ());"
           << "mysql::value_traits< " << type << " >::set_image (" << endl
           << "i." << var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << var << "size = static_cast<unsigned long> (size);"
           << "i." << var << "null = is_null;"
           << "grew = grew || (cap != i." << var << "value.capacity ());"
           << "}";
      }

    private:
      string type;
      string member;
    };

    //
    //
    struct init_value_member: member_base
    {
      init_value_member (context& c)
          : member_base (c, false)
      {
      }

      virtual void
      pre (type& m)
      {
        type = m.type ().fq_name (m.belongs ().hint ());

        os << "// " << m.name () << endl
           << "//" << endl;
      }

      virtual void
      traverse_integer (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << ", i." << var << "value, " <<
          "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_float (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << ", i." << var << "value, " <<
          "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_decimal (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << "," << endl
           << "i." << var << "value," << endl
           << "i." << var << "size," << endl
           << "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_date_time (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << ", i." << var << "value, " <<
          "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_short_string (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << "," << endl
           << "i." << var << "value," << endl
           << "i." << var << "size," << endl
           << "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_long_string (type& m, sql_type const&)
      {
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << "," << endl
           << "i." << var << "value.data ()," << endl
           << "i." << var << "size," << endl
           << "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_bit (type& m, sql_type const&)
      {
        // Represented as a BLOB.
        //
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << "," << endl
           << "i." << var << "value," << endl
           << "i." << var << "size," << endl
           << "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_enum (type& m, sql_type const&)
      {
        // Represented as a string.
        //
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << "," << endl
           << "i." << var << "value.data ()," << endl
           << "i." << var << "size," << endl
           << "i." << var << "null);"
           << endl;
      }

      virtual void
      traverse_set (type& m, sql_type const&)
      {
        // Represented as a string.
        //
        os << "mysql::value_traits< " << type << " >::set_value (" << endl
           << "o." << m.name () << "," << endl
           << "i." << var << "value.data ()," << endl
           << "i." << var << "size," << endl
           << "i." << var << "null);"
           << endl;
      }

    private:
      string type;
    };

    //
    //
    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c),
            init_image_member_ (c, false),
            init_id_image_member_ (c, true),
            init_value_member_ (c)
      {
        *this >> init_image_member_names_ >> init_image_member_;
        *this >> init_id_image_member_names_ >> init_id_image_member_;
        *this >> init_value_member_names_ >> init_value_member_;
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (!c.count ("object"))
          return;

        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        id_member_.traverse (c);
        semantics::data_member& id (*id_member_.member ());

        member_count_.traverse (c);
        size_t column_count (member_count_.count ());

        bool has_grow;
        {
          has_grow_member m (*this);
          traversal::names n (m);
          names (c, n);
          has_grow = m.result ();
        }

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        // query columns
        //
        if (options.generate_query ())
        {
          query_column t (*this, c);
          traversal::names n (t);
          names (c, n);
        }

        // persist_statement
        //
        os << "const char* const " << traits << "::persist_statement =" << endl
           << "\"INSERT INTO `" << table_name (c) << "` (\"" << endl;

        {
          member_column m (*this);
          traversal::names n (m);
          names (c, n);
        }

        os << "\"" << endl
           << "\") VALUES (";

        for (size_t i (0); i < column_count; ++i)
          os << (i != 0 ? "," : "") << '?';

        os << ")\";"
           << endl;

        // find_statement
        //
        os << "const char* const " << traits << "::find_statement =" << endl
           << "\"SELECT \"" << endl;

        {
          member_column m (*this);
          traversal::names n (m);
          names (c, n);
        }

        os << "\"" << endl
           << "\" FROM `" << table_name (c) << "` WHERE `" <<
          column_name (id) << "` = ?\";"
           << endl;

        // update_statement
        //
        os << "const char* const " << traits << "::update_statement =" << endl
           << "\"UPDATE `" << table_name (c) << "` SET \"" << endl;

        {
          member_column m (*this, " = ?");
          traversal::names n (m);
          names (c, n);
        }

        os << "\"" << endl
           << "\" WHERE `" << column_name (id) << "` = ?\";"
           << endl;

        // erase_statement
        //
        os << "const char* const " << traits << "::erase_statement =" << endl
           << "\"DELETE FROM `" << table_name (c) << "`\"" << endl
           << "\" WHERE `" << column_name (id) << "` = ?\";"
           << endl;

        // query_clause
        //
        if (options.generate_query ())
        {
          os << "const char* const " << traits << "::query_clause =" << endl
             << "\"SELECT \"" << endl;

          {
            member_column m (*this);
            traversal::names n (m);
            names (c, n);
          }

          os << "\"" << endl
             << "\" FROM `" << table_name (c) << "` \";"
             << endl;
        }

        // grow ()
        //
        if (has_grow)
        {
          os << "bool " << traits << "::" << endl
             << "grow (image_type& i, my_bool* e)"
             << "{"
             << "bool r (false);"
             << endl;

          {
            grow_member m (*this);
            traversal::names n (m);
            names (c, n);
          }

          os << "return r;"
             << "}";
        }

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (mysql::binding& b, image_type& i)"
           << "{";

        {
          bind_member m (*this, false);
          traversal::names n (m);
          names (c, n);
        }

        os << "b.version++;"
           << "}";

        // bind (id_image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (mysql::binding& b, id_image_type& i)"
           << "{";

        {
          bind_member m (*this, true);
          traversal::names n (m);
          names (c, n);
        }

        os << "b.version++;"
           << "}";

        // init (image, object)
        //
        os << "bool " << traits << "::" << endl
           << "init (image_type& i, object_type& o)"
           << "{"
           << "bool grew (false);"
           << "bool is_null;"
           << endl;
        names (c, init_image_member_names_);
        os << "return grew;"
           << "}";

        // init (object, image)
        //
        os << "void " << traits << "::" << endl
           << "init (object_type& o, image_type& i)"
           << "{";
        names (c, init_value_member_names_);
        os << "}";

        // persist ()
        //
        os << "void " << traits << "::" << endl
           << "persist (database&, object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << "binding& b (sts.image_binding ());"
           << endl;

        if (id.count ("auto"))
          os << "obj." << id.name () << " = 0;";

        os << endl
           << "if (init (sts.image (), obj) || b.version == 0)" << endl
           << "bind (b, sts.image ());"
           << endl
           << "mysql::persist_statement& st (sts.persist_statement ());"
           << "st.execute ();";

        if (id.count ("auto"))
          os << "obj." << id.name () << " = static_cast<id_type> (st.id ());";

        os << "}";

        // update ()
        //
        os << "void " << traits << "::" << endl
           << "update (database&, object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl
           << "bool is_null, grew (false);"
           << "const id_type& id (object_traits::id (obj));"
           << "id_image_type& i (sts.id_image ());";
        names (c, init_id_image_member_names_);
        os << "binding& idb (sts.id_image_binding ());"
           << "if (grew || idb.version == 0)" << endl
           << "bind (idb, i);"
           << endl
           << "binding& imb (sts.image_binding ());"
           << "if (init (sts.image (), obj) || imb.version == 0)" << endl
           << "bind (imb, sts.image ());"
           << endl
           << "sts.update_statement ().execute ();"
           << "}";

        // erase ()
        //
        os << "void " << traits << "::" << endl
           << "erase (database&, const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl
           << "bool is_null, grew (false);"
           << "id_image_type& i (sts.id_image ());";
        names (c, init_id_image_member_names_);
        os << "binding& idb (sts.id_image_binding ());"
           << "if (grew || idb.version == 0)" << endl
           << "bind (idb, i);"
           << endl
           << "sts.erase_statement ().execute ();"
           << "}";

        // find ()
        //
        os << traits << "::pointer_type" << endl
           << traits << "::" << endl
           << "find (database&, const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl
           << "if (find (sts, id))"
           << "{"
           << "pointer_type p (access::object_factory< " << type <<
          " >::create ());"
           << "pointer_traits< pointer_type >::guard g (p);"
           << "init (pointer_traits< pointer_type >::get_ref (p), " <<
          "sts.image ());"
           << "g.release ();"
           << "return p;"
           << "}"
           << "return pointer_type ();"
           << "}";

        os << "bool " << traits << "::" << endl
           << "find (database&, const id_type& id, object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements<object_type>& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl
           << "if (find (sts, id))"
           << "{"
           << "init (obj, sts.image ());"
           << "return true;"
           << "}"
           << "return false;"
           << "}";

        os << "bool " << traits << "::" << endl
           << "find (mysql::object_statements<object_type>& sts, " <<
          "const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "bool is_null, grew (false);"
           << "id_image_type& i (sts.id_image ());";
        names (c, init_id_image_member_names_);
        os << "binding& idb (sts.id_image_binding ());"
           << "if (grew || idb.version == 0)" << endl
           << "bind (idb, i);"
           << endl
           << "binding& imb (sts.image_binding ());"
           << "if (imb.version == 0)" << endl
           << "bind (imb, sts.image ());"
           << endl
           << "mysql::find_statement& st (sts.find_statement ());"
           << "mysql::find_statement::result r (st.execute ());"
           << endl
           << "if (r == mysql::find_statement::no_data)" << endl
           << "return false;"
           << endl;

        if (has_grow)
          os << "if (r == mysql::find_statement::truncated)"
             << "{"
             << "if (grow (sts.image (), sts.image_error ()))"
             << "{"
             << "bind (imb, sts.image ());"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "st.free_result ();"
           << "return true;"
           << "}";

        // query ()
        //
        if (options.generate_query ())
        {
          os << "result< " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query (database&, const query_type& q)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "connection& conn (mysql::transaction::current ().connection ());"
             << "object_statements<object_type>& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << endl
             << "binding& imb (sts.image_binding ());"
             << "if (imb.version == 0)" << endl
             << "bind (imb, sts.image ());"
             << endl
             << "details::shared_ptr<query_statement> st (" << endl
             << "new (details::shared) query_statement (conn," << endl
             << "query_clause + q.clause ()," << endl
             << "imb," << endl
             << "q.parameters ()));"
             << "st->execute ();"
             << endl
             << "details::shared_ptr<odb::result_impl<object_type> > r (" << endl
             << "new (details::shared) mysql::result_impl<object_type> (st, sts));"
             << "return result<object_type> (r);"
             << "}";
        }
      }

    private:
      id_member id_member_;
      member_count member_count_;

      init_image_member init_image_member_;
      traversal::names init_image_member_names_;

      init_image_member init_id_image_member_;
      traversal::names init_id_image_member_names_;

      init_value_member init_value_member_;
      traversal::names init_value_member_names_;
    };
  }

  void
  generate_source (context& ctx)
  {
    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::namespace_ ns;
    class_ c (ctx);

    unit >> unit_defines >> ns;
    unit_defines >> c;

    traversal::defines ns_defines;

    ns >> ns_defines >> ns;
    ns_defines >> c;

    ctx.os << "#include <odb/mysql/mysql.hxx>" << endl
           << "#include <odb/mysql/traits.hxx>" << endl
           << "#include <odb/mysql/database.hxx>" << endl
           << "#include <odb/mysql/transaction.hxx>" << endl
           << "#include <odb/mysql/connection.hxx>" << endl
           << "#include <odb/mysql/statement.hxx>" << endl
           << "#include <odb/mysql/exceptions.hxx>" << endl;

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/mysql/result.hxx>" << endl
             << endl
             << "#include <odb/details/shared-ptr.hxx>" << endl;

    ctx.os << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
