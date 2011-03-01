// file      : odb/mysql/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <map>
#include <set>
#include <vector>
#include <sstream>

#include <odb/mysql/common.hxx>
#include <odb/mysql/schema.hxx>
#include <odb/mysql/source.hxx>

using namespace std;

namespace mysql
{
  namespace
  {
    struct schema_emitter: emitter, context
    {
      schema_emitter (context& c): context (c) {}

      virtual void
      pre ()
      {
        first_ = true;
        os << "db.execute (";
      }

      virtual void
      line (const std::string& l)
      {
        if (first_)
          first_ = false;
        else
          os << endl;

        os << strlit (l);
      }

      virtual void
      post ()
      {
        os << ");" << endl;
      }

    private:
      bool first_;
    };

    struct object_columns: object_columns_base, context
    {
      object_columns (context& c,
                      std::string const& table_name,
                      bool out,
                      char const* suffix = "")
          : object_columns_base (c),
            context (c),
            table_name_ (table_name),
            out_ (out),
            first_ (true),
            suffix_ (suffix)
      {
      }

      object_columns (context& c,
                      std::string const& table_name,
                      bool out,
                      bool first,
                      char const* suffix = "")
          : object_columns_base (c),
            context (c),
            table_name_ (table_name),
            out_ (out),
            first_ (first),
            suffix_ (suffix)
      {
      }

      virtual bool
      column (semantics::data_member& m, string const& name, bool first)
      {
        semantics::data_member* im (inverse (m));

        // Ignore inverse object pointers if we are generating 'in' columns.
        //
        if (im != 0 && !out_)
          return false;

        if (!first || !first_)
          os << ",\"" << endl;

        // Inverse object pointers come from a joined table.
        //
        if (im != 0)
        {
          semantics::class_* c (object_pointer (m.type ()));

          if (container (im->type ()))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name.
            //
            table_prefix tp (table_name (*c) + "_", 1);
            string const& it (table_name (*im, tp));
            string const& id (column_name (*im, "id", "object_id"));

            os << "\"`" << it << "`.`" << id << "`" << suffix_;
          }
          else
          {
            os << "\"`" << table_name (*c) << "`.`" <<
              column_name (id_member (*c)) << "`" << suffix_;
          }
        }
        else
          os << "\"`" << table_name_ << "`.`" << name << "`" << suffix_;

        return true;
      }

    private:
      string table_name_;
      bool out_;
      bool first_;
      string suffix_;
    };

    struct object_joins: object_columns_base, context
    {
      object_joins (context& c, semantics::class_& scope, bool query)
          : object_columns_base (c),
            context (c),
            query_ (query),
            table_ (table_name (scope)),
            id_ (id_member (scope))
      {
      }

      size_t
      count () const
      {
        return joins_.size ();
      }

      void
      write ()
      {
        for (joins::iterator i (joins_.begin ()); i != joins_.end (); ++i)
        {
          if (i->table.empty ())
            continue;

          os << "\" LEFT JOIN `" << i->table << "` ON ";

          for (conditions::iterator b (i->cond.begin ()), j (b);
               j != i->cond.end (); ++j)
          {
            if (j != b)
              os << " OR ";

            os << *j;
          }

          os << "\"" << endl;
        }
      }

      virtual bool
      column (semantics::data_member& m, string const& col_name, bool)
      {
        semantics::class_* c (object_pointer (m.type ()));

        if (c == 0)
          return true;

        string t, dt;
        ostringstream cond, dcond;

        if (semantics::data_member* im = inverse (m))
        {
          if (container (im->type ()))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name.
            //
            string const& ct (table_name (*c));
            table_prefix tp (ct + "_", 1);
            t = table_name (*im, tp);
            string const& val (column_name (*im, "value", "value"));

            cond << "`" << t << "`.`" << val << "` = `" <<
              table_ << "`.`" << column_name (id_) << "`";

            // Add the join for the object itself so that we are able to
            // use it in the WHERE clause.
            //
            if (query_)
            {
              dt = ct;
              string const& id (column_name (*im, "id", "object_id"));

              dcond << "`" << dt << "`.`" << column_name (id_member (*c)) <<
                "` = `" << t << "`.`" << id << "`";
            }
          }
          else
          {
            t = table_name (*c);

            cond << "`" << t << "`.`" << column_name (*im) << "` = `" <<
              table_ << "`.`" << column_name (id_) << "`";
          }
        }
        else if (query_)
        {
          // We need the join to be able to use the referenced object
          // in the WHERE clause.
          //
          t = table_name (*c);

          cond << "`" << t << "`.`" << column_name (id_member (*c)) <<
            "` = `" << table_ << "`.`" << col_name << "`";
        }

        if (!t.empty ())
        {
          size_t i;
          table_map::iterator it (table_map_.find (t));

          if (it != table_map_.end ())
            i = it->second;
          else
          {
            i = joins_.size ();
            joins_.push_back (join ());
            table_map_[t] = i;
          }

          joins_[i].table = t;
          joins_[i].cond.insert (cond.str ());
        }

        if (!dt.empty ())
        {
          // Add dependent join. If one already exists, move it to the
          // bottom.
          //
          size_t i;
          table_map::iterator it (table_map_.find (dt));

          if (it != table_map_.end ())
          {
            i = joins_.size ();
            joins_.push_back (join ());
            joins_[it->second].swap (joins_.back ());
            it->second = i;
          }
          else
          {
            i = joins_.size ();
            joins_.push_back (join ());
            table_map_[dt] = i;
          }

          joins_[i].table = dt;
          joins_[i].cond.insert (dcond.str ());
        }

        return true;
      }

    private:
      bool query_;
      string table_;
      semantics::data_member& id_;

      typedef set<string> conditions;

      struct join
      {
        string table;
        conditions cond;

        void
        swap (join& o)
        {
          table.swap (o.table);
          cond.swap (o.cond);
        }
      };

      typedef vector<join> joins;
      typedef map<string, size_t> table_map;

      joins joins_;
      table_map table_map_;
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

    //
    // bind
    //

    struct bind_member: member_base
    {
      bind_member (context& c,
                   string const& var = string (),
                   string const& arg = string ())
          : member_base (c, var), arg_override_ (arg)
      {
      }

      bind_member (context& c,
                   string const& var,
                   string const& arg,
                   semantics::type& t,
                   string const& fq_type,
                   string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix), arg_override_ (arg)
      {
      }

      virtual bool
      pre (member_info& mi)
      {
        if (container (mi.t))
          return false;

        ostringstream ostr;
        ostr << "b[n]";
        b = ostr.str ();

        arg = arg_override_.empty () ? string ("i") : arg_override_;

        if (var_override_.empty ())
        {
          os << "// " << mi.m.name () << endl
             << "//" << endl;

          if (inverse (mi.m, key_prefix_))
            os << "if (out)"
               << "{";
        }

        return true;
      }

      virtual void
      post (member_info& mi)
      {
        if (var_override_.empty ())
        {
          if (semantics::class_* c = comp_value (mi.t))
            os << "n += " << in_column_count (*c) << "UL;";
          else
            os << "n++;";

          if (inverse (mi.m, key_prefix_))
            os << "}";
          else
            os << endl;
        }
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << "composite_value_traits< " << mi.fq_type () <<
          " >::bind (b + n, " << arg << "." << mi.var << "value);";
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        // While the is_unsigned should indicate whether the
        // buffer variable is unsigned, rather than whether the
        // database type is unsigned, in case of the image types,
        // this is the same.
        //
        os << b << ".buffer_type = " <<
          integer_buffer_types[mi.st->type - sql_type::TINYINT] << ";"
           << b << ".is_unsigned = " << (mi.st->unsign ? "1" : "0") << ";"
           << b << ".buffer = &" << arg << "." << mi.var << "value;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << b << ".buffer_type = " <<
          float_buffer_types[mi.st->type - sql_type::FLOAT] << ";"
           << b << ".buffer = &" << arg << "." << mi.var << "value;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        os << b << ".buffer_type = MYSQL_TYPE_NEWDECIMAL;"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << b << ".buffer_type = " <<
          date_time_buffer_types[mi.st->type - sql_type::DATE] << ";"
           << b << ".buffer = &" << arg << "." << mi.var << "value;";

        if (mi.st->type == sql_type::YEAR)
          os << b << ".is_unsigned = 0;";

        os << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // MySQL documentation is quite confusing about the use of
        // buffer_length and length when it comes to input parameters.
        // Source code, however, tells us that it uses buffer_length
        // only if length is NULL.
        //
        os << b << ".buffer_type = " <<
          char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << b << ".buffer_type = " <<
          char_bin_buffer_types[mi.st->type - sql_type::CHAR] << ";"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Treated as a BLOB.
        //
        os << b << ".buffer_type = MYSQL_TYPE_BLOB;"
           << b << ".buffer = " << arg << "." << mi.var << "value;"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "sizeof (" << arg << "." << mi.var << "value));"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << b << ".buffer_type = MYSQL_TYPE_STRING;"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << b << ".buffer_type = MYSQL_TYPE_STRING;"
           << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
           << b << ".buffer_length = static_cast<unsigned long> (" << endl
           << "" << arg << "." << mi.var << "value.capacity ());"
           << b << ".length = &" << arg << "." << mi.var << "size;"
           << b << ".is_null = &" << arg << "." << mi.var << "null;";
      }

    private:
      string b;
      string arg;
      string arg_override_;
    };

    struct bind_base: traversal::class_, context
    {
      bind_base (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        // Ignore transient bases.
        //
        if (!(c.count ("object") || comp_value (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "composite_value_traits< " << c.fq_name () <<
          " >::bind (b + n, i);"
           << "n += " << in_column_count (c) << "UL;"
           << endl;
      }
    };

    //
    // grow
    //

    struct grow_member: member_base
    {
      grow_member (context& c, size_t& index)
          : member_base (c), index_ (index)
      {
      }

      grow_member (context& c,
                   size_t& index,
                   string const& var,
                   semantics::type& t,
                   string const& fq_type,
                   string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix), index_ (index)
      {
      }

      virtual bool
      pre (member_info& mi)
      {
        if (container (mi.t))
          return false;

        ostringstream ostr;
        ostr << "e[" << index_ << "UL]";
        e = ostr.str ();

        if (var_override_.empty ())
          os << "// " << mi.m.name () << endl
             << "//" << endl;

        return true;
      }

      virtual void
      post (member_info& mi)
      {
        if (semantics::class_* c = comp_value (mi.t))
          index_ += in_column_count (*c);
        else
          index_++;
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << "if (composite_value_traits< " << mi.fq_type () <<
          " >::grow (" << endl
           << "i." << mi.var << "value, e + " << index_ << "UL))"
           << "{"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_integer (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_float (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        // @@ Optimization disabled.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_date_time (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // @@ Optimization disabled.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_bit (member_info&)
      {
        os << e << " = 0;"
           << endl;
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << "if (" << e << ")" << endl
           << "{"
           << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
           << "grew = true;"
           << "}";
      }

    private:
      string e;
      size_t& index_;
    };

    struct grow_base: traversal::class_, context
    {
      grow_base (context& c, size_t& index)
          : context (c), index_ (index)
      {
      }

      virtual void
      traverse (type& c)
      {
        // Ignore transient bases.
        //
        if (!(c.count ("object") || comp_value (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "if (composite_value_traits< " << c.fq_name () <<
          " >::grow (i, e + " << index_ << "UL))"
           << "{"
           << "grew = true;"
           << "}";

        index_ += in_column_count (c);
      }

    private:
      size_t& index_;
    };

    //
    // init image
    //

    struct init_image_member: member_base
    {
      init_image_member (context& c,
                         string const& var = string (),
                         string const& member = string ())
          : member_base (c, var),
            member_image_type_ (c),
            member_database_type_ (c),
            member_override_ (member)
      {
      }

      init_image_member (context& c,
                         string const& var,
                         string const& member,
                         semantics::type& t,
                         string const& fq_type,
                         string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix),
            member_image_type_ (c, t, fq_type, key_prefix),
            member_database_type_ (c, t, fq_type, key_prefix),
            member_override_ (member)
      {
      }

      virtual bool
      pre (member_info& mi)
      {
        // Ignore containers (they get their own table) and inverse
        // object pointers (they are not present in the 'in' binding).
        //
        if (container (mi.t) || inverse (mi.m, key_prefix_))
          return false;

        if (!member_override_.empty ())
          member = member_override_;
        else
        {
          string const& name (mi.m.name ());
          member = "o." + name;

          os << "// " << name << endl
             << "//" << endl;
        }

        if (comp_value (mi.t))
          traits = "composite_value_traits< " + mi.fq_type () + " >";
        else
        {
          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          semantics::type& mt (member_type (mi.m, key_prefix_));

          if (semantics::class_* c = object_pointer (mt))
          {
            type = "obj_traits::id_type";
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);

            // Handle NULL pointers and extract the id.
            //
            os << "{"
               << "typedef object_traits< " << c->fq_name () <<
              " > obj_traits;";

            if (weak_pointer (mt))
            {
              os << "typedef pointer_traits< " << mi.fq_type () <<
                " > wptr_traits;"
                 << "typedef pointer_traits< wptr_traits::" <<
                "strong_pointer_type > ptr_traits;"
                 << endl
                 << "wptr_traits::strong_pointer_type sp (" <<
                "wptr_traits::lock (" << member << "));";

              member = "sp";
            }
            else
              os << "typedef pointer_traits< " << mi.fq_type () <<
              " > ptr_traits;"
                 << endl;

            os << "bool is_null (ptr_traits::null_ptr (" << member << "));"
               << "if (!is_null)"
               << "{"
               << "const " << type << "& id (" << endl;

            if (lazy_pointer (mt))
              os << "ptr_traits::object_id< ptr_traits::element_type  > (" <<
                member << ")";
            else
              os << "obj_traits::id (ptr_traits::get_ref (" << member << "))";

            os << ");"
               << endl;

            member = "id";
          }
          else
          {
            type = mi.fq_type ();
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);

            os << "{"
               << "bool is_null;";
          }

          traits = "mysql::value_traits<\n    "
            + type + ",\n    "
            + image_type + ",\n    "
            + db_type + " >";
        }

        return true;
      }

      virtual void
      post (member_info& mi)
      {
        if (!comp_value (mi.t))
        {
          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          if (object_pointer (member_type (mi.m, key_prefix_)))
          {
            os << "}";

            if (!null_pointer (mi.m, key_prefix_))
              os << "else" << endl
                 << "throw null_pointer ();";
          }

          os << "i." << mi.var << "null = is_null;"
             << "}";
        }
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << "if (" << traits << "::init (i." << mi.var << "value, " <<
          member << "))"
           << "{"
           << "grew = true;"
           << "}";
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        os << traits << "::set_image (" << endl
           << "i." << mi.var << "value, is_null, " << member << ");";
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << traits << "::set_image (" << endl
           << "i." << mi.var << "value, is_null, " << member << ");";
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        // @@ Optimization: can remove growth check if buffer is fixed.
        //
        os << "std::size_t size (0);"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << traits << "::set_image (" << endl
           << "i." << mi.var << "value, is_null, " << member << ");";
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        // @@ Optimization: can remove growth check if buffer is fixed.
        //
        os << "std::size_t size (0);"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << "std::size_t size (0);"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Represented as a BLOB.
        //
        os << "std::size_t size (0);"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "sizeof (i." << mi.var << "value)," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);";
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << "std::size_t size (0);"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << "std::size_t size (0);"
           << "std::size_t cap (i." << mi.var << "value.capacity ());"
           << traits << "::set_image (" << endl
           << "i." << mi.var << "value," << endl
           << "size," << endl
           << "is_null," << endl
           << member << ");"
           << "i." << mi.var << "size = static_cast<unsigned long> (size);"
           << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
      }

    private:
      string type;
      string db_type;
      string member;
      string image_type;
      string traits;

      member_image_type member_image_type_;
      member_database_type member_database_type_;

      string member_override_;
    };

    struct init_image_base: traversal::class_, context
    {
      init_image_base (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        // Ignore transient bases.
        //
        if (!(c.count ("object") || comp_value (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "if (composite_value_traits< " << c.fq_name () <<
          " >::init (i, o))"
           << "{"
           << "grew = true;"
           << "}";
      }
    };

    //
    // init value
    //

    struct init_value_member: member_base
    {
      init_value_member (context& c, string const& member = string ())
          : member_base (c),
            member_image_type_ (c),
            member_database_type_ (c),
            member_override_ (member)
      {
      }

      init_value_member (context& c,
                         string const& var,
                         string const& member,
                         semantics::type& t,
                         string const& fq_type,
                         string const& key_prefix)
          : member_base (c, var, t, fq_type, key_prefix),
            member_image_type_ (c, t, fq_type, key_prefix),
            member_database_type_ (c, t, fq_type, key_prefix),
            member_override_ (member)
      {
      }

      virtual bool
      pre (member_info& mi)
      {
        if (container (mi.t))
          return false;

        if (!member_override_.empty ())
          member = member_override_;
        else
        {
          string const& name (mi.m.name ());
          member = "o." + name;

          os << "// " << name << endl
             << "//" << endl;
        }

        if (comp_value (mi.t))
          traits = "composite_value_traits< " + mi.fq_type () + " >";
        else
        {
          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          semantics::type& mt (member_type (mi.m, key_prefix_));

          if (semantics::class_* c = object_pointer (mt))
          {
            type = "obj_traits::id_type";
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);

            // Handle NULL pointers and extract the id.
            //
            os << "{"
               << "typedef object_traits< " << c->fq_name () <<
              " > obj_traits;"
               << "typedef pointer_traits< " << mi.fq_type () <<
              " > ptr_traits;"
               << endl
               << "if (i." << mi.var << "null)" << endl;

            if (null_pointer (mi.m, key_prefix_))
              os << member << " = ptr_traits::pointer_type ();";
            else
              os << "throw null_pointer ();";

            os << "else"
               << "{"
               << type << " id;";

            member = "id";
          }
          else
          {
            type = mi.fq_type ();
            image_type = member_image_type_.image_type (mi.m);
            db_type = member_database_type_.database_type (mi.m);
          }

          traits = "mysql::value_traits<\n    "
            + type + ",\n    "
            + image_type + ",\n    "
            + db_type + " >";
        }

        return true;
      }

      virtual void
      post (member_info& mi)
      {
        if (comp_value (mi.t))
          return;

        // When handling a pointer, mi.t is the id type of the referenced
        // object.
        //
        semantics::type& mt (member_type (mi.m, key_prefix_));

        if (object_pointer (mt))
        {
          member = member_override_.empty ()
            ? "o." + mi.m.name ()
            : member_override_;

          if (lazy_pointer (mt))
            os << member << " = ptr_traits::pointer_type (db, id);";
          else
            os << "// If a compiler error points to the line below, then" << endl
               << "// it most likely means that a pointer used in a member" << endl
               << "// cannot be initialized from an object pointer." << endl
               << "//" << endl
               << member << " = ptr_traits::pointer_type (" << endl
               << "db.load< ptr_traits::element_type > (id));";

          os << "}"
             << "}";
        }
      }

      virtual void
      traverse_composite (member_info& mi)
      {
        os << traits << "::init (" << member << ", i." <<
          mi.var << "value, db);"
           << endl;
      }

      virtual void
      traverse_integer (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << ", i." << mi.var << "value, " <<
          "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_float (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << ", i." << mi.var << "value, " <<
          "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_decimal (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_date_time (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << ", i." << mi.var << "value, " <<
          "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_short_string (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_long_string (member_info& mi)
      {
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_bit (member_info& mi)
      {
        // Represented as a BLOB.
        //
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_enum (member_info& mi)
      {
        // Represented as a string.
        //
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

      virtual void
      traverse_set (member_info& mi)
      {
        // Represented as a string.
        //
        os << traits << "::set_value (" << endl
           << member << "," << endl
           << "i." << mi.var << "value," << endl
           << "i." << mi.var << "size," << endl
           << "i." << mi.var << "null);"
           << endl;
      }

    private:
      string type;
      string db_type;
      string image_type;
      string traits;
      string member;

      member_image_type member_image_type_;
      member_database_type member_database_type_;

      string member_override_;
    };

    struct init_value_base: traversal::class_, context
    {
      init_value_base (context& c)
          : context (c)
      {
      }

      virtual void
      traverse (type& c)
      {
        // Ignore transient bases.
        //
        if (!(c.count ("object") || comp_value (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "composite_value_traits< " << c.fq_name () <<
          " >::init (o, i, db);"
           << endl;
      }
    };

    // Member-specific traits types for container members.
    //
    struct container_traits: object_members_base, context
    {
      container_traits (context& c, semantics::class_& obj)
          : object_members_base (c, true, true),
            context (c),
            object_ (obj),
            id_member_ (id_member (obj))
      {
        obj_scope_ = "access::object_traits< " + obj.fq_name () + " >";
      }

      virtual void
      container (semantics::data_member& m)
      {
        using semantics::type;

        type& t (m.type ());
        container_kind_type ck (container_kind (t));

        type& vt (container_vt (t));
        type* it (0);
        type* kt (0);

        semantics::data_member* im (context::inverse (m, "value"));

        bool ordered (false);
        bool inverse (im != 0);
        bool grow (false);

        switch (ck)
        {
        case ck_ordered:
          {
            if (!unordered (m))
            {
              it = &container_it (t);
              ordered = true;
              grow = grow || context::grow (m, *it, "index");
            }
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            kt = &container_kt (t);
            grow = grow || context::grow (m, *kt, "key");
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        grow = grow || context::grow (m, vt, "value");

        bool eager_ptr (is_a (m, test_eager_pointer, vt, "value") ||
                        has_a (vt, test_eager_pointer));

        string name (prefix_ + public_name (m) + "_traits");
        string scope (obj_scope_ + "::" + name);

        os << "// " << m.name () << endl
           << "//" << endl
           << endl;

        //
        // Statements.
        //
        string table (table_name (m, table_prefix_));

        // select_all_statement
        //
        os << "const char* const " << scope <<
          "::select_all_statement =" << endl;

        if (inverse)
        {
          semantics::class_* c (object_pointer (vt));

          string inv_table; // Other table name.
          string inv_id;    // Other id column.
          string inv_fid;   // Other foreign id column (ref to us).

          if (context::container (im->type ()))
          {
            // many(i)-to-many
            //

            // This other container is a direct member of the class so the
            // table prefix is just the class table name.
            //
            table_prefix tp (table_name (*c) + "_", 1);
            inv_table = table_name (*im, tp);
            inv_id = column_name (*im, "id", "object_id");
            inv_fid = column_name (*im, "value", "value");
          }
          else
          {
            // many(i)-to-one
            //
            inv_table = table_name (*c);
            inv_id = column_name (id_member (*c));
            inv_fid = column_name (*im);
          }

          os << "\"SELECT \"" << endl
             << "\"`" << inv_fid << "`,\"" << endl
             << "\"`" << inv_id << "`\"" << endl
             << "\" FROM `" << inv_table << "` WHERE `" <<
            inv_fid << "` = ?\"";
        }
        else
        {
          os << "\"SELECT \"" << endl
             << "\"`" << column_name (m, "id", "object_id") << "`";

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                os << ",\"" << endl
                   << "\"`" << column_name (m, "index", "index") << "`";
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              if (semantics::class_* ckt = comp_value (*kt))
              {
                object_columns t (*this, table, false, false);
                t.traverse_composite (m, *ckt, "key", "key");
              }
              else
              {
                os << ",\"" << endl
                   << "\"`" << column_name (m, "key", "key") << "`";
              }
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              break;
            }
          }

          if (semantics::class_* cvt = comp_value (vt))
          {
            object_columns t (*this, table, false, false);
            t.traverse_composite (m, *cvt, "value", "value");
          }
          else
          {
            os << ",\"" << endl
               << "\"`" << column_name (m, "value", "value") << "`";
          }

          os << "\"" << endl
             << "\" FROM `" << table << "` WHERE `" <<
            column_name (m, "id", "object_id") << "` = ?\"" << endl;

          if (ordered)
            os << "\" ORDER BY `" << column_name (m, "index", "index") <<
              "`\"";
        }

        os << ";"
           << endl;

        // insert_one_statement
        //
        os << "const char* const " << scope <<
          "::insert_one_statement =" << endl;

        if (inverse)
          os << " \"\";"
             << endl;
        else
        {
          os << "\"INSERT INTO `" << table << "` (\"" << endl
             << "\"`" << column_name (m, "id", "object_id") << "`";

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                os << ",\"" << endl
                   << "\"`" << column_name (m, "index", "index") << "`";
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              if (semantics::class_* ckt = comp_value (*kt))
              {
                object_columns t (*this, table, false, false);
                t.traverse_composite (m, *ckt, "key", "key");
              }
              else
              {
                os << ",\"" << endl
                   << "\"`" << column_name (m, "key", "key") << "`";
              }
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              break;
            }
          }

          if (semantics::class_* cvt = comp_value (vt))
          {
            object_columns t (*this, table, false, false);
            t.traverse_composite (m, *cvt, "value", "value");
          }
          else
          {
            os << ",\"" << endl
               << "\"`" << column_name (m, "value", "value") << "`";
          }

          os << "\"" << endl
             << "\") VALUES (";

          for (size_t i (0), n (m.get<size_t> ("data-column-count")); i < n; ++i)
            os << (i != 0 ? "," : "") << '?';

          os << ")\";"
             << endl;
        }

        // delete_all_statement
        //
        os << "const char* const " << scope <<
          "::delete_all_statement =" << endl;

        if (inverse)
          os << " \"\";"
             << endl;
        else
        {
          os << "\"DELETE FROM `" << table << "`\"" << endl
             << "\" WHERE `" << column_name (m, "id", "object_id") << "` = ?\";"
             << endl;
        }

        //
        // Functions.
        //

        // bind()
        //
        {
          bind_member bind_id (*this, "id_", "id");

          // bind (cond_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (MYSQL_BIND* b, id_image_type* p, cond_image_type& c)"
             << "{"
             << "ODB_POTENTIALLY_UNUSED (c);"
             << endl
             << "std::size_t n (0);"
             << endl;

          os << "// object_id" << endl
             << "//" << endl
             << "if (p != 0)"
             << "{"
             << "id_image_type& id (*p);";
          bind_id.traverse (id_member_);
          os << "}"
             << "n++;"
             << endl;

          // We don't need to update the bind index since this is the
          // last element.
          //
          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                os << "// index" << endl
                   << "//" << endl;
                bind_member bm (
                  *this, "index_", "c", *it, "index_type", "index");
                bm.traverse (m);
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              bind_member bm (*this, "key_", "c", *kt, "key_type", "key");
              bm.traverse (m);
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "// value" << endl
                 << "//" << endl;
              bind_member bm (*this, "value_", "c", vt, "value_type", "value");
              bm.traverse (m);
              break;
            }
          }

          os << "}";

          // bind (data_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (MYSQL_BIND* b, id_image_type* p, data_image_type& d)"
             << "{"
             << "size_t n (0);"
             << endl;

          os << "// object_id" << endl
             << "//" << endl
             << "if (p != 0)"
             << "{"
             << "id_image_type& id (*p);";
          bind_id.traverse (id_member_);
          os << "}"
             << "n++;"
             << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                os << "// index" << endl
                   << "//" << endl;
                bind_member bm (
                  *this, "index_", "d", *it, "index_type", "index");
                bm.traverse (m);
                os << "n++;" // Simple value.
                   << endl;
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              bind_member bm (*this, "key_", "d", *kt, "key_type", "key");
              bm.traverse (m);

              if (semantics::class_* c = comp_value (*kt))
                os << "n += " << in_column_count (*c) << "UL;"
                   << endl;
              else
                os << "n++;"
                   << endl;
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              break;
            }
          }

          // We don't need to update the bind index since this is the
          // last element.
          //
          os << "// value" << endl
             << "//" << endl;
          bind_member bm (*this, "value_", "d", vt, "value_type", "value");
          bm.traverse (m);

          os << "}";
        }

        // grow ()
        //
        {
          size_t index (0);

          os << "void " << scope << "::" << endl
             << "grow (data_image_type& i, my_bool* e)"
             << "{"
             << "bool grew (false);"
             << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                os << "// index" << endl
                   << "//" << endl;
                grow_member gm (
                  *this, index, "index_", *it, "index_type", "index");
                gm.traverse (m);
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              grow_member gm (*this, index, "key_", *kt, "key_type", "key");
              gm.traverse (m);
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              break;
            }
          }

          os << "// value" << endl
             << "//" << endl;
          grow_member gm (*this, index, "value_", vt, "value_type", "value");
          gm.traverse (m);

          os << "if (grew)" << endl
             << "i.version++;"
             << "}";
        }

        // init (data_image)
        //
        if (!inverse)
        {
          os << "void " << scope << "::" << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
                os << "init (data_image_type& i, index_type j, " <<
                  "const value_type& v)";
              else
                os << "init (data_image_type& i, const value_type& v)";

              os<< "{"
                << "bool grew (false);"
                << endl;

              if (ordered)
              {
                os << "// index" << endl
                   << "//" << endl;

                init_image_member im (
                  *this, "index_", "j", *it, "index_type", "index");
                im.traverse (m);
              }

              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "init (data_image_type& i, const key_type& k, " <<
                "const value_type& v)"
                 << "{"
                 << "bool grew (false);"
                 << endl
                 << "// key" << endl
                 << "//" << endl;

              init_image_member im (
                *this, "key_", "k", *kt, "key_type", "key");
              im.traverse (m);

              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "init (data_image_type& i, const value_type& v)"
                 << "{"
                 << "bool grew (false);"
                 << endl;
              break;
            }
          }

          os << "// value" << endl
             << "//" << endl;
          {
            init_image_member im (
              *this, "value_", "v", vt, "value_type", "value");
            im.traverse (m);
          }

          os << "if (grew)" << endl
             << "i.version++;"
             << "}";
        }

        // init (data)
        //
        os << "void " << scope << "::" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            if (ordered)
              os << "init (index_type& j, value_type& v, " <<
                "const data_image_type& i, database& db)";
            else
              os << "init (value_type& v, const data_image_type& i, " <<
                "database& db)";

            os << "{"
               << "ODB_POTENTIALLY_UNUSED (db);"
               << endl;

            if (ordered)
            {
              os << "// index" << endl
                 << "//" << endl;

              init_value_member im (
                *this, "index_", "j", *it, "index_type", "index");
              im.traverse (m);
            }

            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (key_type& k, value_type& v, " <<
              "const data_image_type& i, database& db)"
               << "{"
               << "ODB_POTENTIALLY_UNUSED (db);"
               << endl
               << "// key" << endl
               << "//" << endl;

            init_value_member im (*this, "key_", "k", *kt, "key_type", "key");
            im.traverse (m);

            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (value_type& v, const data_image_type& i, " <<
              "database& db)"
               << "{"
               << "ODB_POTENTIALLY_UNUSED (db);"
               << endl;
            break;
          }
        }

        os << "// value" << endl
           << "//" << endl;
        {
          // If the value is an object pointer, pass the id type as a
          // type override.
          //
          init_value_member im (
            *this, "value_", "v", vt, "value_type", "value");
          im.traverse (m);
        }
        os << "}";

        // insert_one
        //
        {
          string ia, ka, va, da;

          if (!inverse)
          {
            ia = ordered ? " i" : "";
            ka = " k";
            va = " v";
            da = " d";
          }

          os << "void " << scope << "::" << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              os << "insert_one (index_type" << ia << ", " <<
                "const value_type&" << va << ", " <<
                "void*" << da << ")";
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "insert_one (const key_type&" << ka << ", " <<
                "const value_type&" << va << ", " <<
                "void*" << da << ")";
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "insert_one (const value_type&" << va << ", " <<
                "void*" << da << ")";
              break;
            }
          }

          os << "{";

          if (!inverse)
          {
            os << "using namespace mysql;"
               << endl
               << "typedef container_statements< " << name << " > statements;"
               << "statements& sts (*static_cast< statements* > (d));"
               << "binding& b (sts.data_image_binding ());"
               << "data_image_type& di (sts.data_image ());"
               << endl;

            switch (ck)
            {
            case ck_ordered:
              {
                os << "init (di, " << (ordered ? "i, " : "") << "v);";
                break;
              }
            case ck_map:
            case ck_multimap:
              {
                os << "init (di, k, v);";
                break;
              }
            case ck_set:
            case ck_multiset:
              {
                os << "init (di, v);";
                break;
              }
            }

            os << endl
               << "if (di.version != sts.data_image_version () || " <<
              "b.version == 0)"
               << "{"
               << "bind (b.bind, 0, di);"
               << "sts.data_image_version (di.version);"
               << "b.version++;"
               << "}"
               << "if (!sts.insert_one_statement ().execute ())" << endl
               << "throw object_already_persistent ();";
          }

          os << "}";
        }


        // load_all
        //
        os << "bool " << scope << "::" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "load_all (index_type&" << (ordered ? " i" : "") <<
              ", value_type& v, void* d)";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "load_all (key_type& k, value_type& v, void* d)";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "load_all (value_type& v, void* d)";
            break;
          }
        }

        os << "{"
           << "using namespace mysql;"
           << endl
           << "typedef container_statements< " << name << " > statements;"
           << "statements& sts (*static_cast< statements* > (d));"
           << "data_image_type& di (sts.data_image ());";

        // Extract current element.
        //
        switch (ck)
        {
        case ck_ordered:
          {
            os << "init (" << (ordered ? "i, " : "") <<
              "v, di, sts.connection ().database ());"
               << endl;
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (k, v, di, sts.connection ().database ());"
               << endl;
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (v, di, sts.connection ().database ());"
               << endl;
            break;
          }
        }

        // If we are loading an eager pointer, then the call to init
        // above executes other statements which potentially could
        // change the image.
        //
        if (eager_ptr)
        {
          os << "id_image_type& ii (sts.id_image ());"
             << endl
             << "if (di.version != sts.data_image_version () ||" << endl
             << "ii.version != sts.data_id_image_version ())"
             << "{"
             << "binding& b (sts.data_image_binding ());"
             << "bind (b.bind, &ii, di);"
             << "sts.data_image_version (di.version);"
             << "sts.data_id_image_version (ii.version);"
             << "b.version++;"
             << "}";
        }

        // Fetch next.
        //
        os << "select_statement& st (sts.select_all_statement ());"
           << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "grow (di, sts.data_image_error ());"
             << endl
             << "if (di.version != sts.data_image_version ())"
             << "{"
             << "binding& b (sts.data_image_binding ());"
             << "bind (b.bind, 0, di);"
             << "sts.data_image_version (di.version);"
             << "b.version++;"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "if (r == select_statement::no_data)"
           << "{"
           << "st.free_result ();"
           << "return false;"
           << "}"
           << "return true;"
           << "}";

        // delete_all
        //
        os << "void " << scope << "::" << endl
           << "delete_all (void*" << (inverse ? "" : " d") << ")"
           << "{";

        if (!inverse)
          os << "using namespace mysql;"
             << endl
             << "typedef container_statements< " << name << " > statements;"
             << "statements& sts (*static_cast< statements* > (d));"
             << "sts.delete_all_statement ().execute ();";

        os << "}";

        // persist
        //
        if (!inverse)
        {
          os << "void " << scope << "::" << endl
             << "persist (const container_type& c," << endl
             << "id_image_type& id," << endl
             << "statements_type& sts)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "binding& b (sts.data_image_binding ());"
             << "if (id.version != sts.data_id_image_version () || " <<
            "b.version == 0)"
             << "{"
             << "bind (b.bind, &id, sts.data_image ());"
             << "sts.data_id_image_version (id.version);"
             << "b.version++;"
             << "}"
             << "sts.id_image (id);"
             << "functions_type& fs (sts.functions ());";

          if (ck == ck_ordered)
            os << "fs.ordered (" << (ordered ? "true" : "false") << ");";

          os << "container_traits::persist (c, fs);"
             << "}";
        }

        // load
        //
        os << "void " << scope << "::" << endl
           << "load (container_type& c," << endl
           << "id_image_type& id," << endl
           << "statements_type& sts)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "binding& db (sts.data_image_binding ());"
           << "if (id.version != sts.data_id_image_version () || db.version == 0)"
           << "{"
           << "bind (db.bind, &id, sts.data_image ());"
           << "sts.data_id_image_version (id.version);"
           << "db.version++;"
           << "}"
           << "binding& cb (sts.cond_image_binding ());"
           << "if (id.version != sts.cond_id_image_version () || cb.version == 0)"
           << "{"
           << "bind (cb.bind, &id, sts.cond_image ());"
           << "sts.cond_id_image_version (id.version);"
           << "cb.version++;"
           << "}"
           << "select_statement& st (sts.select_all_statement ());"
           << "st.execute ();";

        // If we are loading eager object pointers, cache the result
        // since we will be loading other objects.
        //
        if (eager_ptr)
          os << "st.cache ();";

        os << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "data_image_type& di (sts.data_image ());"
             << "grow (di, sts.data_image_error ());"
             << endl
             << "if (di.version != sts.data_image_version ())"
             << "{"
             << "bind (db.bind, 0, sts.data_image ());"
             << "sts.data_image_version (di.version);"
             << "db.version++;"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "bool more (r != select_statement::no_data);"
           << endl
           << "if (!more)" << endl
           << "st.free_result ();"
           << endl
           << "sts.id_image (id);"
           << "functions_type& fs (sts.functions ());";

        if (ck == ck_ordered)
          os << "fs.ordered (" << (ordered ? "true" : "false") << ");";

        os << "container_traits::load (c, more, fs);"
           << "}";

        // update
        //
        if (!inverse)
        {
          os << "void " << scope << "::" << endl
             << "update (const container_type& c," << endl
             << "id_image_type& id," << endl
             << "statements_type& sts)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "binding& db (sts.data_image_binding ());"
             << "if (id.version != sts.data_id_image_version () || " <<
            "db.version == 0)"
             << "{"
             << "bind (db.bind, &id, sts.data_image ());"
             << "sts.data_id_image_version (id.version);"
             << "db.version++;"
             << "}"
            //
            // We may need cond if the specialization calls delete_all.
            //
             << "binding& cb (sts.cond_image_binding ());"
             << "if (id.version != sts.cond_id_image_version () || " <<
            "cb.version == 0)"
             << "{"
             << "bind (cb.bind, &id, sts.cond_image ());"
             << "sts.cond_id_image_version (id.version);"
             << "cb.version++;"
             << "}"
             << "sts.id_image (id);"
             << "functions_type& fs (sts.functions ());";

          if (ck == ck_ordered)
            os << "fs.ordered (" << (ordered ? "true" : "false") << ");";

          os << "container_traits::update (c, fs);"
             << "}";
        }

        // erase
        //
        if (!inverse)
        {
          os << "void " << scope << "::" << endl
             << "erase (id_image_type& id, statements_type& sts)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "binding& b (sts.cond_image_binding ());"
             << "if (id.version != sts.cond_id_image_version () || b.version == 0)"
             << "{"
             << "bind (b.bind, &id, sts.cond_image ());"
             << "sts.cond_id_image_version (id.version);"
             << "b.version++;"
             << "}"
             << "sts.id_image (id);"
             << "functions_type& fs (sts.functions ());";

          if (ck == ck_ordered)
            os << "fs.ordered (" << (ordered ? "true" : "false") << ");";

          os << "container_traits::erase (fs);"
             << "}";
        }
      }

    private:
      string obj_scope_;
      semantics::class_& object_;
      semantics::data_member& id_member_;
    };

    // Container statement cache members.
    //
    struct container_cache_members: object_members_base, context
    {
      container_cache_members (context& c)
          : object_members_base (c, true, false), context (c)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        string traits (prefix_ + public_name (m) + "_traits");
        os << "mysql::container_statements< " << traits << " > " <<
          prefix_ << m.name () << ";";
      }
    };

    struct container_cache_init_members: object_members_base, context
    {
      container_cache_init_members (context& c)
          : object_members_base (c, true, false), context (c), first_ (true)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        if (first_)
        {
          os << endl
             << ": ";
          first_ = false;
        }
        else
          os << "," << endl
             << "  ";

        os << prefix_ << m.name () << " (c)";
      }

    private:
      bool first_;
    };

    // Calls for container members.
    //
    struct container_calls: object_members_base, context
    {
      enum call_type
      {
        persist_call,
        load_call,
        update_call,
        erase_call
      };

      container_calls (context& c, call_type call)
          : object_members_base (c, true, false), context (c), call_ (call)
      {
      }

      virtual void
      composite (semantics::data_member& m, semantics::class_& c)
      {
        string old (obj_prefix_);
        obj_prefix_ += m.name ();
        obj_prefix_ += '.';
        object_members_base::composite (m, c);
        obj_prefix_ = old;
      }

      virtual void
      container (semantics::data_member& m)
      {
        using semantics::type;

        bool inverse (context::inverse (m, "value"));

        string const& name (m.name ());
        string obj_name (obj_prefix_ + name);
        string sts_name (prefix_ + name);
        string traits (prefix_ + public_name (m) + "_traits");

        switch (call_)
        {
        case persist_call:
          {
            if (!inverse)
              os << traits << "::persist (" << endl
                 << "obj." << obj_name << "," << endl
                 << "i," << endl
                 << "sts.container_statment_cache ()." << sts_name << ");"
                 << endl;
            break;
          }
        case load_call:
          {
            os << traits << "::load (" << endl
               << "obj." << obj_name << "," << endl
               << "i," << endl
               << "sts.container_statment_cache ()." << sts_name << ");"
               << endl;
            break;
          }
        case update_call:
          {
            if (!inverse)
              os << traits << "::update (" << endl
                 << "obj." << obj_name << "," << endl
                 << "i," << endl
                 << "sts.container_statment_cache ()." << sts_name << ");"
                 << endl;
            break;
          }
        case erase_call:
          {
            if (!inverse)
              os << traits << "::erase (" << endl
                 << "i," << endl
                 << "sts.container_statment_cache ()." << sts_name << ");"
                 << endl;
            break;
          }
        }
      }

    private:
      call_type call_;
      string obj_prefix_;
    };

    //
    //
    struct class_: traversal::class_, context
    {
      class_ (context& c)
          : context (c),
            grow_base_ (c, index_),
            grow_member_ (c, index_),
            bind_base_ (c),
            bind_member_ (c),
            bind_id_member_ (c, "id_"),
            init_image_base_ (c),
            init_image_member_ (c),
            init_id_image_member_ (c, "id_", "id"),
            init_value_base_ (c),
            init_value_member_ (c),
            init_id_value_member_ (c, "id"),

            schema_emitter_ (c),
            schema_drop_ (c, schema_emitter_),
            schema_create_ (c, schema_emitter_)
      {
        grow_base_inherits_ >> grow_base_;
        grow_member_names_ >> grow_member_;

        bind_base_inherits_ >> bind_base_;
        bind_member_names_ >> bind_member_;

        init_image_base_inherits_ >> init_image_base_;
        init_image_member_names_ >> init_image_member_;

        init_value_base_inherits_ >> init_value_base_;
        init_value_member_names_ >> init_value_member_;
      }

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (c.count ("object"))
          traverse_object (c);
        else if (comp_value (c))
          traverse_value (c);
      }

      virtual void
      traverse_object (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        bool grow (context::grow (c));
        bool def_ctor (TYPE_HAS_DEFAULT_CONSTRUCTOR (c.tree_node ()));

        semantics::data_member& id (id_member (c));
        bool auto_id (id.count ("auto"));
        bool grow_id (context::grow (id));

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        //
        // Containers.
        //
        bool straight_containers (has_a (c, test_straight_container));
        bool containers (straight_containers || has_a (c, test_container));

        // Statement cache (definition).
        //
        {
          os << "struct " << traits << "::container_statement_cache_type"
             << "{";

          container_cache_members cm (*this);
          cm.traverse (c);

          os << (containers ? "\n" : "")
             << "container_statement_cache_type (mysql::connection&" <<
            (containers ? " c" : "") << ")";

          container_cache_init_members im (*this);
          im.traverse (c);

          os << "{"
             << "}"
             << "};";
        }

        // Traits types.
        //
        if (containers)
        {
          container_traits t (*this, c);
          t.traverse (c);
        }

        // query columns
        //
        if (options.generate_query ())
        {
          query_columns t (*this, c);
          t.traverse (c);
        }

        string const& table (table_name (c));

        // persist_statement
        //
        os << "const char* const " << traits << "::persist_statement =" << endl
           << "\"INSERT INTO `" << table << "` (\"" << endl;

        {
          object_columns t (*this, table, false);
          t.traverse (c);
        }

        os << "\"" << endl
           << "\") VALUES (";

        for (size_t i (0), n (in_column_count (c)); i < n; ++i)
          os << (i != 0 ? "," : "") << '?';

        os << ")\";"
           << endl;

        // find_statement
        //
        os << "const char* const " << traits << "::find_statement =" << endl
           << "\"SELECT \"" << endl;

        {
          object_columns t (*this, table, true);
          t.traverse (c);
        }

        os << "\"" << endl
           << "\" FROM `" << table << "`\"" << endl;

        {
          object_joins t (*this, c, false);
          t.traverse (c);
          t.write ();
        }

        os << "\" WHERE `" << table << "`.`" << column_name (id) << "` = ?\";"
           << endl;

        // update_statement
        //
        os << "const char* const " << traits << "::update_statement =" << endl
           << "\"UPDATE `" << table << "` SET \"" << endl;

        {
          object_columns t (*this, table, false, " = ?");
          t.traverse (c);
        }

        os << "\"" << endl
           << "\" WHERE `" << table << "`.`" << column_name (id) << "` = ?\";"
           << endl;

        // erase_statement
        //
        os << "const char* const " << traits << "::erase_statement =" << endl
           << "\"DELETE FROM `" << table << "`\"" << endl
           << "\" WHERE `" << table << "`.`" << column_name (id) << "` = ?\";"
           << endl;

        // query_clause
        //
        if (options.generate_query ())
        {
          object_joins oj (*this, c, true);
          oj.traverse (c);

          // We only need DISTINCT if there are joins (object pointers)
          // and can optimize it out otherwise.
          //
          os << "const char* const " << traits << "::query_clause =" << endl
             << "\"SELECT " << (oj.count () ? "DISTINCT " : "") << "\"" << endl;

          {
            object_columns oc (*this, table, true);
            oc.traverse (c);
          }

          os << "\"" << endl
             << "\" FROM `" << table << "`\"" << endl;

          oj.write ();

          os << "\" \";"
             << endl;
        }

        // id
        //
        if (options.generate_query ())
        {
          os << traits << "::id_type" << endl
             << traits << "::" << endl
             << "id (const image_type& i)"
             << "{"
             << "id_type id;";
          init_id_value_member_.traverse (id);
          os << "return id;"
             << "}";
        }

        // grow ()
        //
        os << "void " << traits << "::" << endl
           << "grow (image_type& i, my_bool* e)"
           << "{"
           << "bool grew (false);"
           << endl;

        index_ = 0;
        inherits (c, grow_base_inherits_);
        names (c, grow_member_names_);

        os << "if (grew)" << endl
           << "i.version++;" << endl
           << "}";

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND* b, image_type& i, bool out)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (out);"
           << endl
           << "std::size_t n (0);"
           << endl;

        inherits (c, bind_base_inherits_);
        names (c, bind_member_names_);

        os << "}";

        // bind (id_image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND* b, id_image_type& i)"
           << "{"
           << "std::size_t n (0);";
        bind_id_member_.traverse (id);
        os << "}";

        // init (image, object)
        //
        os << "void " << traits << "::" << endl
           << "init (image_type& i, const object_type& o)"
           << "{"
           << "bool grew (false);"
           << endl;

        inherits (c, init_image_base_inherits_);
        names (c, init_image_member_names_);

        os << "if (grew)" << endl
           << "i.version++;" << endl
           << "}";

        // init (object, image)
        //
        os << "void " << traits << "::" << endl
           << "init (object_type& o, const image_type& i, database& db)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << endl;

        inherits (c, init_value_base_inherits_);
        names (c, init_value_member_names_);

        os << "}";

        // init (id_image, id)
        //
        os << "void " << traits << "::" << endl
           << "init (id_image_type& i, const id_type& id)"
           << "{";

        if (grow_id)
          os << "bool grew (false);";

        init_id_image_member_.traverse (id);

        if (grow_id)
          os << endl
             << "if (grew)" << endl
             << "i.version++;";

        os << "}";

        // persist ()
        //
        os << "void " << traits << "::" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << "image_type& im (sts.image ());"
           << "binding& imb (sts.in_image_binding ());"
           << endl;

        if (auto_id)
          os << "obj." << id.name () << " = 0;";

        os << "init (im, obj);"
           << endl
           << "if (im.version != sts.in_image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im, false);"
           << "sts.in_image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "insert_statement& st (sts.persist_statement ());"
           << "if (!st.execute ())" << endl
           << "throw object_already_persistent ();"
           << endl;

        if (auto_id)
          os << "obj." << id.name () << " = static_cast<id_type> (st.id ());"
             << endl;

        if (straight_containers)
        {
          // Initialize id_image.
          //
          os << "id_image_type& i (sts.id_image ());"
             << "init (i, obj." << id.name () << ");"
             << endl;

          container_calls t (*this, container_calls::persist_call);
          t.traverse (c);
        }

        os << "}";

        // update ()
        //
        os << "void " << traits << "::" << endl
           << "update (database&, const object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl;

        // Initialize id image.
        //
        os << "id_image_type& i (sts.id_image ());"
           << "init (i, obj." << id.name () << ");"
           << endl;

        os << "binding& idb (sts.id_image_binding ());"
           << "if (i.version != sts.id_image_version () || idb.version == 0)"
           << "{"
           << "bind (idb.bind, i);"
           << "sts.id_image_version (i.version);"
           << "idb.version++;"
           << "}";

        // Initialize data image.
        //
        os << "image_type& im (sts.image ());"
           << "binding& imb (sts.in_image_binding ());"
           << "init (im, obj);"
           << endl
           << "if (im.version != sts.in_image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im, false);"
           << "sts.in_image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "sts.update_statement ().execute ();";

          if (straight_containers)
          {
            os << endl;
            container_calls t (*this, container_calls::update_call);
            t.traverse (c);
          }

        os << "}";

        // erase ()
        //
        os << "void " << traits << "::" << endl
           << "erase (database&, const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << endl;

        // Initialize id image.
        //
        os << "id_image_type& i (sts.id_image ());"
           << "init (i, id);"
           << endl;

        os << "binding& idb (sts.id_image_binding ());"
           << "if (i.version != sts.id_image_version () || idb.version == 0)"
           << "{"
           << "bind (idb.bind, i);"
           << "sts.id_image_version (i.version);"
           << "idb.version++;"
           << "}"
           << "if (sts.erase_statement ().execute () != 1)" << endl
           << "throw object_not_persistent ();";

        if (straight_containers)
        {
          os << endl;
          container_calls t (*this, container_calls::erase_call);
          t.traverse (c);
        }

        os << "}";

        // find ()
        //
        if (def_ctor)
        {
          os << traits << "::pointer_type" << endl
             << traits << "::" << endl
             << "find (database& db, const id_type& id)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "connection& conn (mysql::transaction::current ().connection ());"
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << "object_statements< object_type >::auto_lock l (sts);"
             << endl
             << "if (l.locked ())"
             << "{"
             << "if (!find_ (sts, id))" << endl
             << "return pointer_type ();"
             << "}"
             << "pointer_type p (" << endl
             << "access::object_factory< object_type, pointer_type  >::create ());"
             << "pointer_traits< pointer_type >::guard pg (p);"
             << "pointer_cache_traits< pointer_type >::insert_guard ig (" << endl
             << "pointer_cache_traits< pointer_type >::insert (db, id, p));"
             << "object_type& obj (pointer_traits< pointer_type >::get_ref (p));"
             << endl
             << "if (l.locked ())"
             << "{"
             << "init (obj, sts.image (), db);"
             << "load_ (sts, obj);"
             << "sts.load_delayed ();"
             << "l.unlock ();"
             << "}"
             << "else" << endl
             << "sts.delay_load (id, obj, ig.position ());"
             << endl;

          os << "ig.release ();"
             << "pg.release ();"
             << "return p;"
             << "}";
        }

        os << "bool " << traits << "::" << endl
           << "find (database& db, const id_type& id, object_type& obj)"
           << "{"
           << "using namespace mysql;"
           << endl
           << "connection& conn (mysql::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << "object_statements< object_type >::auto_lock l (sts);"
           << endl
           << "if (l.locked ())"
           << "{"
           << "if (!find_ (sts, id))" << endl
           << "return false;"
           << "}"
           << "reference_cache_traits< object_type >::insert_guard ig (" << endl
           << "reference_cache_traits< object_type >::insert (db, id, obj));"
           << endl
           << "if (l.locked ())"
           << "{"
           << "init (obj, sts.image (), db);"
           << "load_ (sts, obj);"
           << "sts.load_delayed ();"
           << "l.unlock ();"
           << "}"
           << "else" << endl
           << "sts.delay_load (id, obj, ig.position ());"
           << endl;

        os << "ig.release ();"
           << "return true;"
           << "}";

        //
        //
        os << "bool " << traits << "::" << endl
           << "find_ (mysql::object_statements< object_type >& sts, " <<
          "const id_type& id)"
           << "{"
           << "using namespace mysql;"
           << endl;

        // Initialize id image.
        //
        os << "id_image_type& i (sts.id_image ());"
           << "init (i, id);"
           << endl;

        os << "binding& idb (sts.id_image_binding ());"
           << "if (i.version != sts.id_image_version () || idb.version == 0)"
           << "{"
           << "bind (idb.bind, i);"
           << "sts.id_image_version (i.version);"
           << "idb.version++;"
           << "}";

        // Rebind data image.
        //
        os << "image_type& im (sts.image ());"
           << "binding& imb (sts.out_image_binding ());"
           << endl
           << "if (im.version != sts.out_image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im, true);"
           << "sts.out_image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "select_statement& st (sts.find_statement ());"
           << "st.execute ();"
           << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "grow (im, sts.out_image_error ());"
             << endl
             << "if (im.version != sts.out_image_version ())"
             << "{"
             << "bind (imb.bind, im, true);"
             << "sts.out_image_version (im.version);"
             << "imb.version++;"
             << "st.refetch ();"
             << "}"
             << "}";

        os << "st.free_result ();"
           << "return r != select_statement::no_data;"
           << "}";

        // load_()
        //
        if (containers)
        {
          os << "void " << traits << "::" << endl
             << "load_ (mysql::object_statements< object_type >& sts, " <<
            "object_type& obj)"
             << "{"
             << "id_image_type& i (sts.id_image ());"
             << endl;
          container_calls t (*this, container_calls::load_call);
          t.traverse (c);
          os << "}";
        }

        // query ()
        //
        if (options.generate_query ())
        {
          os << "template<>" << endl
             << "result< " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query< " << traits << "::object_type > (" << endl
             << "database& db," << endl
             << "const query_type& q)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "connection& conn (mysql::transaction::current ().connection ());"
             << endl
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << "details::shared_ptr<select_statement> st;"
             << endl
             << "query_ (db, q, sts, st);"
             << endl
             << "details::shared_ptr<odb::result_impl<object_type> > r (" << endl
             << "new (details::shared) mysql::result_impl<object_type> (st, sts));"
             << "return result<object_type> (r);"
             << "}";

          os << "template<>" << endl
             << "result< const " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query< const " << traits << "::object_type > (" << endl
             << "database& db," << endl
             << "const query_type& q)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "connection& conn (mysql::transaction::current ().connection ());"
             << endl
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << "details::shared_ptr<select_statement> st;"
             << endl
             << "query_ (db, q, sts, st);"
             << endl
             << "details::shared_ptr<odb::result_impl<const object_type> > r (" << endl
             << "new (details::shared) mysql::result_impl<const object_type> (st, sts));"
             << "return result<const object_type> (r);"
             << "}";

          os << "void " << traits << "::" << endl
             << "query_ (database&," << endl
             << "const query_type& q," << endl
             << "mysql::object_statements< object_type >& sts,"
             << "details::shared_ptr<mysql::select_statement>& st)"
             << "{"
             << "using namespace mysql;"
             << endl
             << "image_type& im (sts.image ());"
             << "binding& imb (sts.out_image_binding ());"
             << endl
             << "if (im.version != sts.out_image_version () || imb.version == 0)"
             << "{"
             << "bind (imb.bind, im, true);"
             << "sts.out_image_version (im.version);"
             << "imb.version++;"
             << "}"
             << "st.reset (new (details::shared) select_statement (" << endl
             << "sts.connection ()," << endl
             << "query_clause + q.clause ()," << endl
             << "q.parameters ()," << endl
             << "imb));"
             << "st->execute ();"
             << "}";
        }

        // create_schema ()
        //
        if (embedded_schema)
        {
          os << "void " << traits << "::" << endl
             << "create_schema (database& db)"
             << "{";

          schema_drop_.traverse (c);
          schema_create_.traverse (c);

          os << "}";

          os << "static const schema_catalog_entry" << endl
             << "schema_catalog_entry_" << flat_name (type) << "_ (" << endl
             << strlit (options.default_schema ()) << "," << endl
             << "&" << traits << "::create_schema);"
             << endl;
        }
      }

      virtual void
      traverse_value (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::composite_value_traits< " + type + " >");

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        // grow ()
        //
        os << "bool " << traits << "::" << endl
           << "grow (image_type& i, my_bool* e)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (e);"
           << endl
           << "bool grew (false);"
           << endl;

        index_ = 0;
        inherits (c, grow_base_inherits_);
        names (c, grow_member_names_);

        os << "return grew;"
           << "}";

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (MYSQL_BIND* b, image_type& i)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (b);"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << endl
           << "std::size_t n (0);"
           << "ODB_POTENTIALLY_UNUSED (n);"
           << endl;

        inherits (c, bind_base_inherits_);
        names (c, bind_member_names_);

        os << "}";

        // init (image, object)
        //
        os << "bool " << traits << "::" << endl
           << "init (image_type& i, const value_type& o)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << endl
           << "bool grew (false);"
           << endl;

        inherits (c, init_image_base_inherits_);
        names (c, init_image_member_names_);

        os << "return grew;"
           << "}";

        // init (object, image)
        //
        os << "void " << traits << "::" << endl
           << "init (value_type& o, const image_type&  i, database& db)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << endl;

        inherits (c, init_value_base_inherits_);
        names (c, init_value_member_names_);

        os << "}";
      }

    private:
      bool id_;
      size_t index_;

      grow_base grow_base_;
      traversal::inherits grow_base_inherits_;
      grow_member grow_member_;
      traversal::names grow_member_names_;

      bind_base bind_base_;
      traversal::inherits bind_base_inherits_;
      bind_member bind_member_;
      traversal::names bind_member_names_;
      bind_member bind_id_member_;

      init_image_base init_image_base_;
      traversal::inherits init_image_base_inherits_;
      init_image_member init_image_member_;
      traversal::names init_image_member_names_;

      init_image_member init_id_image_member_;

      init_value_base init_value_base_;
      traversal::inherits init_value_base_inherits_;
      init_value_member init_value_member_;
      traversal::names init_value_member_names_;
      init_value_member init_id_value_member_;

      schema_emitter schema_emitter_;
      class_drop schema_drop_;
      class_create schema_create_;
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

    //
    //
    ctx.os << "#include <odb/cache-traits.hxx>" << endl;

    if (ctx.embedded_schema)
      ctx.os << "#include <odb/schema-catalog-impl.hxx>" << endl;

    ctx.os << endl;

    //
    //
    ctx.os << "#include <odb/mysql/mysql.hxx>" << endl
           << "#include <odb/mysql/traits.hxx>" << endl
           << "#include <odb/mysql/database.hxx>" << endl
           << "#include <odb/mysql/transaction.hxx>" << endl
           << "#include <odb/mysql/connection.hxx>" << endl
           << "#include <odb/mysql/statement.hxx>" << endl
           << "#include <odb/mysql/statement-cache.hxx>" << endl
           << "#include <odb/mysql/object-statements.hxx>" << endl
           << "#include <odb/mysql/container-statements.hxx>" << endl
           << "#include <odb/mysql/exceptions.hxx>" << endl;

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/mysql/result.hxx>" << endl;

    ctx.os << endl;

    // Details includes.
    //
    ctx.os << "#include <odb/details/unused.hxx>" << endl;

    if (ctx.options.generate_query ())
      ctx.os << "#include <odb/details/shared-ptr.hxx>" << endl;

    ctx.os << endl;

    ctx.os << "namespace odb"
           << "{";

    unit.dispatch (ctx.unit);

    ctx.os << "}";
  }
}
