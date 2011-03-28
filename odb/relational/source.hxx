// file      : odb/relational/source.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SOURCE_HXX
#define ODB_RELATIONAL_SOURCE_HXX

#include <map>
#include <set>
#include <vector>
#include <sstream>

#include <odb/emitter.hxx>

#include <odb/relational/context.hxx>
#include <odb/relational/common.hxx>
#include <odb/relational/schema.hxx>

namespace relational
{
  namespace source
  {
    struct schema_emitter: emitter, virtual context
    {
      virtual void
      pre ()
      {
        first_ = true;
      }

      virtual void
      line (const std::string& l)
      {
        if (first_)
        {
          first_ = false;
          os << "db.execute (";
        }
        else
          os << endl;

        os << strlit (l);
      }

      virtual void
      post ()
      {
        if (!first_) // Ignore empty statements.
          os << ");" << endl;
      }

    private:
      bool first_;
    };

    struct object_columns: object_columns_base, virtual context
    {
      typedef object_columns base;

      object_columns (bool out,
                      bool last = true,
                      char const* suffix = "")
          : out_ (out), last_ (last), suffix_ (suffix)
      {
      }

      object_columns (std::string const& table_qname,
                      bool out,
                      bool last = true)
          : table_name_ (table_qname), out_ (out), last_ (last)
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

        if (!first)
        {
          line_ += ',';
          os << strlit (line_) << endl;
        }

        line_.clear ();

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
            if (!table_name_.empty ())
            {
              table_prefix tp (table_name (*c) + "_", 1);
              string const& it (table_qname (*im, tp));

              line_ += it;
              line_ += '.';
            }

            line_ += column_qname (*im, "id", "object_id");
          }
          else
          {
            if (!table_name_.empty ())
            {
              line_ += table_qname (*c);
              line_ += '.';
            }

            line_ += column_qname (id_member (*c));
          }
        }
        else
        {
          if (!table_name_.empty ())
          {
            line_ += table_name_;
            line_ += '.';
          }

          line_ += quote_id (name);
        }

        line_ += suffix_;
        return true;
      }

      virtual void
      flush ()
      {
        if (!last_)
          line_ += ',';

        os << strlit (line_) << endl;
      }

    private:
      string table_name_;
      bool out_;
      bool last_;
      string suffix_;
      string line_;
    };

    struct object_joins: object_columns_base, virtual context
    {
      typedef object_joins base;

      //@@ context::object Might have to be create every time.
      //
      object_joins (semantics::class_& scope, bool query)
          : query_ (query),
            table_ (table_qname (scope)),
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

          string line (" LEFT JOIN ");
          line += i->table;
          line += " ON ";

          for (conditions::iterator b (i->cond.begin ()), j (b);
               j != i->cond.end (); ++j)
          {
            if (j != b)
              line += " OR ";

            line += *j;
          }

          os << strlit (line) << endl;
        }
      }

      virtual bool
      column (semantics::data_member& m, string const& col_name, bool)
      {
        semantics::class_* c (object_pointer (m.type ()));

        if (c == 0)
          return true;

        string t, dt;
        std::ostringstream cond, dcond; // @@ diversion?

        if (semantics::data_member* im = inverse (m))
        {
          if (container (im->type ()))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name.
            //
            string const& ct (table_name (*c));
            table_prefix tp (ct + "_", 1);
            t = table_qname (*im, tp);
            string const& val (column_qname (*im, "value", "value"));

            cond << t << '.' << val << " = " << table_ << '.' <<
              column_qname (id_);

            // Add the join for the object itself so that we are able to
            // use it in the WHERE clause.
            //
            if (query_)
            {
              dt = ct;
              string const& id (column_qname (*im, "id", "object_id"));

              dcond << dt << '.' << column_qname (id_member (*c)) << " = " <<
                t << '.' << id;
            }
          }
          else
          {
            t = table_qname (*c);

            cond << t << '.' << column_qname (*im) << " = " <<
              table_ << '.' << column_qname (id_);
          }
        }
        else if (query_)
        {
          // We need the join to be able to use the referenced object
          // in the WHERE clause.
          //
          t = table_qname (*c);

          cond << t << '.' << column_qname (id_member (*c)) << " = " <<
            table_ << '.' << quote_id (col_name);
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

      typedef std::set<string> conditions;

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

      typedef std::vector<join> joins;
      typedef std::map<string, size_t> table_map;

      joins joins_;
      table_map table_map_;
    };

    //
    // bind
    //

    struct bind_member: virtual member_base
    {
      typedef bind_member base;

      bind_member (string const& var = string (),
                   string const& arg = string ())
          : member_base (var, 0, string (), string ()),
            arg_override_ (arg)
      {
      }

      bind_member (string const& var,
                   string const& arg,
                   semantics::type& t,
                   string const& fq_type,
                   string const& key_prefix)
          : member_base (var, &t, fq_type, key_prefix),
            arg_override_ (arg)
      {
      }

    protected:
      string arg_override_;
    };

    struct bind_base: traversal::class_, virtual context
    {
      typedef bind_base base;

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

    struct grow_member: virtual member_base
    {
      typedef grow_member base;

      grow_member (size_t& index)
          : member_base (string (), 0, string (), string ()), index_ (index)
      {
      }

      grow_member (size_t& index,
                   string const& var,
                   semantics::type& t,
                   string const& fq_type,
                   string const& key_prefix)
          : member_base (var, &t, fq_type, key_prefix), index_ (index)
      {
      }

    protected:
      size_t& index_;
    };

    struct grow_base: traversal::class_, virtual context
    {
      typedef grow_base base;

      grow_base (size_t& index): index_ (index) {}

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
          " >::grow (i, t + " << index_ << "UL))"
           << "{"
           << "grew = true;"
           << "}";

        index_ += in_column_count (c);
      }

    protected:
      size_t& index_;
    };

    //
    // init image
    //

    struct init_image_member: virtual member_base
    {
      typedef init_image_member base;

      init_image_member (string const& var = string (),
                         string const& member = string ())
          : member_base (var, 0, string (), string ()),
            member_override_ (member)
      {
      }

      init_image_member (string const& var,
                         string const& member,
                         semantics::type& t,
                         string const& fq_type,
                         string const& key_prefix)
          : member_base (var, &t, fq_type, key_prefix),
            member_override_ (member)
      {
      }

    protected:
      string member_override_;
    };

    struct init_image_base: traversal::class_, virtual context
    {
      typedef init_image_base base;

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

    struct init_value_member: virtual member_base
    {
      typedef init_value_member base;

      init_value_member (string const& member = string ())
          : member_base (string (), 0, string (), string ()),
            member_override_ (member)
      {
      }

      init_value_member (string const& var,
                         string const& member,
                         semantics::type& t,
                         string const& fq_type,
                         string const& key_prefix)
          : member_base (var, &t, fq_type, key_prefix),
            member_override_ (member)
      {
      }

    protected:
      string member_override_;
    };

    struct init_value_base: traversal::class_, virtual context
    {
      typedef init_value_base base;

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
    struct container_traits: object_members_base, virtual context
    {
      typedef container_traits base;

      container_traits (semantics::class_& obj) // @@ context::object
          : object_members_base (true, true),
            object_ (obj),
            id_member_ (id_member (obj))
      {
        obj_scope_ = "access::object_traits< " + obj.fq_name () + " >";
      }

      // Unless the database system can execute several interleaving
      // statements, cache the result set.
      //
      virtual void
      cache_result (string const& statement)
      {
        os << statement << ".cache ();";
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
        string table (table_qname (m, table_prefix_));

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
            inv_table = table_qname (*im, tp);
            inv_id = column_qname (*im, "id", "object_id");
            inv_fid = column_qname (*im, "value", "value");
          }
          else
          {
            // many(i)-to-one
            //
            inv_table = table_qname (*c);
            inv_id = column_qname (id_member (*c));
            inv_fid = column_qname (*im);
          }

          os << strlit ("SELECT ") << endl
             << strlit (inv_table + '.' + inv_fid + ',') << endl
             << strlit (inv_table + '.' + inv_id) << endl
             << strlit (" FROM " + inv_table +
                        " WHERE " + inv_table + '.' + inv_fid + " = ?");
        }
        else
        {
          string const& id_col (column_qname (m, "id", "object_id"));

          os << strlit ("SELECT ") << endl
             << strlit (table + '.' + id_col + ',') << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                string const& col (column_qname (m, "index", "index"));
                os << strlit (table + '.' + col + ',') << endl;
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              if (semantics::class_* ckt = comp_value (*kt))
              {
                instance<object_columns> t (table, false, false);
                t->traverse_composite (m, *ckt, "key", "key");
              }
              else
              {
                string const& col (column_qname (m, "key", "key"));
                os << strlit (table + '.' + col + ',') << endl;
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
            instance<object_columns> t (table, false);
            t->traverse_composite (m, *cvt, "value", "value");
          }
          else
          {
            string const& col (column_qname (m, "value", "value"));
            os << strlit (table + '.' + col) << endl;
          }

          os << strlit (" FROM " + table +
                        " WHERE " + table + '.' + id_col + " = ?") << endl;

          if (ordered)
          {
            string const& col (column_qname (m, "index", "index"));
            os << strlit (" ORDER BY " + table + '.' + col) << endl;
          }
        }

        os << ";"
           << endl;

        // insert_one_statement
        //
        os << "const char* const " << scope <<
          "::insert_one_statement =" << endl;

        if (inverse)
          os << strlit ("") << ";"
             << endl;
        else
        {
          os << strlit ("INSERT INTO " + table + " (") << endl
             << strlit (column_qname (m, "id", "object_id") + ',') << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
                os << strlit (column_qname (m, "index", "index") + ',') <<
                  endl;
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              if (semantics::class_* ckt = comp_value (*kt))
              {
                instance<object_columns> t (false, false);
                t->traverse_composite (m, *ckt, "key", "key");
              }
              else
                os << strlit (column_qname (m, "key", "key") + ',') << endl;
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
            instance<object_columns> t (false);
            t->traverse_composite (m, *cvt, "value", "value");
          }
          else
            os << strlit (column_qname (m, "value", "value")) << endl;

          string values;
          for (size_t i (0), n (m.get<size_t> ("data-column-count"));
               i < n; ++i)
            values += i != 0 ? ",?" : "?";

          os << strlit (") VALUES (" + values + ")") << ";"
             << endl;
        }

        // delete_all_statement
        //
        os << "const char* const " << scope <<
          "::delete_all_statement =" << endl;

        if (inverse)
          os << strlit ("") << ";"
             << endl;
        else
        {
          os << strlit ("DELETE FROM " + table) << endl
             << strlit (" WHERE " + column_qname (m, "id", "object_id") +
                        " = ?") << ";"
             << endl;
        }

        //
        // Functions.
        //

        // bind()
        //
        {
          instance<bind_member> bind_id ("id_", "id");

          // bind (cond_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (" << bind_vector << " b, id_image_type* p, " <<
            "cond_image_type& c)"
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
          bind_id->traverse (id_member_);
          os << "}"
             << "n++;"
             << endl;

          // We don't need to update the bind index since this is the
          // last element.
          //
          // Index/key is currently not used (see also cond_column_count).
          //
#if 0
          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
              {
                os << "// index" << endl
                   << "//" << endl;
                instance<bind_member> bm (
                  "index_", "c", *it, "index_type", "index");
                bm->traverse (m);
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              instance<bind_member> bm ("key_", "c", *kt, "key_type", "key");
              bm->traverse (m);
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "// value" << endl
                 << "//" << endl;
              instance<bind_member> bm (
                "value_", "c", vt, "value_type", "value");
              bm->traverse (m);
              break;
            }
          }
#endif

          os << "}";

          // bind (data_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (" << bind_vector << " b, id_image_type* p, " <<
            "data_image_type& d)"
             << "{"
             << "size_t n (0);"
             << endl;

          os << "// object_id" << endl
             << "//" << endl
             << "if (p != 0)"
             << "{"
             << "id_image_type& id (*p);";
          bind_id->traverse (id_member_);
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
                instance<bind_member> bm (
                  "index_", "d", *it, "index_type", "index");
                bm->traverse (m);
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
              instance<bind_member> bm ("key_", "d", *kt, "key_type", "key");
              bm->traverse (m);

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
          instance<bind_member> bm ("value_", "d", vt, "value_type", "value");
          bm->traverse (m);

          os << "}";
        }

        // grow ()
        //
        {
          size_t index (0);

          os << "void " << scope << "::" << endl
             << "grow (data_image_type& i, " << truncated_vector << " t)"
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
                instance<grow_member> gm (
                  index, "index_", *it, "index_type", "index");
                gm->traverse (m);
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;
              instance<grow_member> gm (index, "key_", *kt, "key_type", "key");
              gm->traverse (m);
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
          instance<grow_member> gm (
            index, "value_", vt, "value_type", "value");
          gm->traverse (m);

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

                instance<init_image_member> im (
                  "index_", "j", *it, "index_type", "index");
                im->traverse (m);
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

              instance<init_image_member> im (
                "key_", "k", *kt, "key_type", "key");
              im->traverse (m);

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
            instance<init_image_member> im (
              "value_", "v", vt, "value_type", "value");
            im->traverse (m);
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

              instance<init_value_member> im (
                "index_", "j", *it, "index_type", "index");
              im->traverse (m);
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

            instance<init_value_member> im (
              "key_", "k", *kt, "key_type", "key");
            im->traverse (m);

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
          instance<init_value_member> im (
            "value_", "v", vt, "value_type", "value");
          im->traverse (m);
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
            os << "using namespace " << db << ";"
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
           << "using namespace " << db << ";"
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
             << "grow (di, sts.data_image_truncated ());"
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
          os << "using namespace " << db << ";"
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
             << "using namespace " << db << ";"
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
           << "using namespace " << db << ";"
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

        // If we are loading eager object pointers, we may need to cache
        // the result since we will be loading other objects.
        //
        if (eager_ptr)
          cache_result ("st");

        os << "select_statement::result r (st.fetch ());";

        if (grow)
          os << endl
             << "if (r == select_statement::truncated)"
             << "{"
             << "data_image_type& di (sts.data_image ());"
             << "grow (di, sts.data_image_truncated ());"
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
             << "using namespace " << db << ";"
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
             << "using namespace " << db << ";"
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

    protected:
      string obj_scope_;
      semantics::class_& object_;
      semantics::data_member& id_member_;
    };

    // Container statement cache members.
    //
    struct container_cache_members: object_members_base, virtual context
    {
      typedef container_cache_members base;

      container_cache_members ()
          : object_members_base (true, false)
      {
      }

      virtual void
      container (semantics::data_member& m)
      {
        string traits (prefix_ + public_name (m) + "_traits");
        os << db << "::container_statements< " << traits << " > " <<
          prefix_ << m.name () << ";";
      }
    };

    struct container_cache_init_members: object_members_base, virtual context
    {
      typedef container_cache_init_members base;

      container_cache_init_members ()
          : object_members_base (true, false), first_ (true)
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

    protected:
      bool first_;
    };

    // Calls for container members.
    //
    struct container_calls: object_members_base, virtual context
    {
      typedef container_calls base;

      enum call_type
      {
        persist_call,
        load_call,
        update_call,
        erase_call
      };

      container_calls (call_type call)
          : object_members_base (true, false), call_ (call)
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

    protected:
      call_type call_;
      string obj_prefix_;
    };

    //
    //
    struct class_: traversal::class_, virtual context
    {
      typedef class_ base;

      class_ ()
          : grow_base_ (index_),
            grow_member_ (index_),
            bind_id_member_ ("id_"),
            init_id_image_member_ ("id_", "id"),
            init_id_value_member_ ("id"),
            schema_drop_ (schema_emitter_),
            schema_create_ (schema_emitter_)
      {
        init ();
      }

      class_ (class_ const&)
          : root_context (), //@@ -Wextra
            context (),
            grow_base_ (index_),
            grow_member_ (index_),
            bind_id_member_ ("id_"),
            init_id_image_member_ ("id_", "id"),
            init_id_value_member_ ("id"),
            schema_drop_ (schema_emitter_),
            schema_create_ (schema_emitter_)
      {
        init ();
      }

      void
      init ()
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
      init_auto_id (semantics::data_member&, // id member
                    string const&)           // image variable prefix
      {
        assert (false);
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

          instance<container_cache_members> cm;
          cm->traverse (c);

          os << (containers ? "\n" : "")
             << "container_statement_cache_type (" << db << "::connection&" <<
            (containers ? " c" : "") << ")";

          instance<container_cache_init_members> im;
          im->traverse (c);

          os << "{"
             << "}"
             << "};";
        }

        // Traits types.
        //
        if (containers)
        {
          instance<container_traits> t (c);
          t->traverse (c);
        }

        // query columns
        //
        if (options.generate_query ())
        {
          instance<query_columns> t (c);
          t->traverse (c);
        }

        string const& table (table_qname (c));
        string const& id_col (column_qname (id));

        // persist_statement
        //
        os << "const char* const " << traits << "::persist_statement =" << endl
           << strlit ("INSERT INTO " + table + " (") << endl;

        {
          instance<object_columns> t (false);
          t->traverse (c);
        }

        string values;
        for (size_t i (0), n (in_column_count (c)); i < n; ++i)
          values += i != 0 ? ",?" : "?";

        os << strlit (") VALUES (" + values + ")") << ";"
           << endl;

        // find_statement
        //
        os << "const char* const " << traits << "::find_statement =" << endl
           << strlit ("SELECT ") << endl;

        {
          instance<object_columns> t (table, true);
          t->traverse (c);
        }

        os << strlit (" FROM " + table) << endl;

        {
          bool f (false);
          instance<object_joins> t (c, f); // @@ (im)perfect forwarding
          t->traverse (c);
          t->write ();
        }

        os << strlit (" WHERE " + table + '.' + id_col + " = ?") << ";"
           << endl;

        // update_statement
        //
        os << "const char* const " << traits << "::update_statement =" << endl
           << strlit ("UPDATE " + table + " SET ") << endl;

        {
          instance<object_columns> t (false, true, " = ?");
          t->traverse (c);
        }

        os << strlit (" WHERE " + table + '.' + id_col + " = ?") << ";"
           << endl;

        // erase_statement
        //
        os << "const char* const " << traits << "::erase_statement =" << endl
           << strlit ("DELETE FROM " + table) << endl
           << strlit (" WHERE " + table + '.' + id_col + " = ?") << ";"
           << endl;

        // query_clause
        //
        if (options.generate_query ())
        {
          bool t (true);
          instance<object_joins> oj (c, t); //@@ (im)perfect forwarding
          oj->traverse (c);

          // We only need DISTINCT if there are joins (object pointers)
          // and can optimize it out otherwise.
          //
          os << "const char* const " << traits << "::query_clause =" << endl
             << strlit (oj->count () ? "SELECT DISTINCT " : "SELECT ") << endl;

          {
            instance<object_columns> oc (table, true);
            oc->traverse (c);
          }

          os << strlit (" FROM " + table) << endl;
          oj->write ();
          os << strlit (" ") << ";"
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
          init_id_value_member_->traverse (id);
          os << "return id;"
             << "}";
        }

        // grow ()
        //
        os << "void " << traits << "::" << endl
           << "grow (image_type& i, " << truncated_vector << " t)"
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
           << "bind (" << bind_vector << " b, image_type& i, bool out)"
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
           << "bind (" << bind_vector << " b, id_image_type& i)"
           << "{"
           << "std::size_t n (0);";
        bind_id_member_->traverse (id);
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

        init_id_image_member_->traverse (id);

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
           << "using namespace " << db << ";"
           << endl
           << "connection& conn (" << db <<
          "::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find<object_type> ());"
           << "image_type& im (sts.image ());"
           << "binding& imb (sts.in_image_binding ());"
           << endl
           << "init (im, obj);";

        if (auto_id)
        {
          string const& n (id.name ());
          string var ("im." + n + (n[n.size () - 1] == '_' ? "" : "_"));
          init_auto_id (id, var);
        }

        os << endl
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

          instance<container_calls> t (container_calls::persist_call);
          t->traverse (c);
        }

        os << "}";

        // update ()
        //
        os << "void " << traits << "::" << endl
           << "update (database&, const object_type& obj)"
           << "{"
           << "using namespace " << db << ";"
           << endl
           << "connection& conn (" << db <<
          "::transaction::current ().connection ());"
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
            instance<container_calls> t (container_calls::update_call);
            t->traverse (c);
          }

        os << "}";

        // erase ()
        //
        os << "void " << traits << "::" << endl
           << "erase (database&, const id_type& id)"
           << "{"
           << "using namespace " << db << ";"
           << endl
           << "connection& conn (" << db <<
          "::transaction::current ().connection ());"
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
          instance<container_calls> t (container_calls::erase_call);
          t->traverse (c);
        }

        os << "}";

        // find ()
        //
        if (c.default_ctor ())
        {
          os << traits << "::pointer_type" << endl
             << traits << "::" << endl
             << "find (database& db, const id_type& id)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << "connection& conn (" << db <<
            "::transaction::current ().connection ());"
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
           << "using namespace " << db << ";"
           << endl
           << "connection& conn (" << db <<
          "::transaction::current ().connection ());"
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
           << "find_ (" << db << "::object_statements< object_type >& sts, " <<
          "const id_type& id)"
           << "{"
           << "using namespace " << db << ";"
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
             << "grow (im, sts.out_image_truncated ());"
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
             << "load_ (" << db << "::object_statements< object_type >& " <<
            "sts, object_type& obj)"
             << "{"
             << "id_image_type& i (sts.id_image ());"
             << endl;
          instance<container_calls> t (container_calls::load_call);
          t->traverse (c);
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
             << "using namespace " << db << ";"
             << endl
             << "connection& conn (" << db <<
            "::transaction::current ().connection ());"
             << endl
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << "details::shared_ptr<select_statement> st;"
             << endl
             << "query_ (db, q, sts, st);"
             << endl
             << "details::shared_ptr<odb::result_impl<object_type> > r (" << endl
             << "new (details::shared) " << db <<
            "::result_impl<object_type> (q, st, sts));"
             << "return result<object_type> (r);"
             << "}";

          os << "template<>" << endl
             << "result< const " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query< const " << traits << "::object_type > (" << endl
             << "database& db," << endl
             << "const query_type& q)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << "connection& conn (" << db <<
            "::transaction::current ().connection ());"
             << endl
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find<object_type> ());"
             << "details::shared_ptr<select_statement> st;"
             << endl
             << "query_ (db, q, sts, st);"
             << endl
             << "details::shared_ptr<odb::result_impl<const object_type> > r (" << endl
             << "new (details::shared) " << db <<
            "::result_impl<const object_type> (q, st, sts));"
             << "return result<const object_type> (r);"
             << "}";

          os << "void " << traits << "::" << endl
             << "query_ (database&," << endl
             << "const query_type& q," << endl
             << db << "::object_statements< object_type >& sts," << endl
             << "details::shared_ptr<" << db << "::select_statement>& st)"
             << "{"
             << "using namespace " << db << ";"
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
             << "q.parameters_binding ()," << endl
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
             << "{"
             << "ODB_POTENTIALLY_UNUSED (db);"
             << endl;

          schema_drop_->traverse (c);
          schema_create_->traverse (c);

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
           << "grow (image_type& i, " << truncated_vector << " t)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (t);"
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
           << "bind (" << bind_vector << " b, image_type& i)"
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

      instance<grow_base> grow_base_;
      traversal::inherits grow_base_inherits_;
      instance<grow_member> grow_member_;
      traversal::names grow_member_names_;

      instance<bind_base> bind_base_;
      traversal::inherits bind_base_inherits_;
      instance<bind_member> bind_member_;
      traversal::names bind_member_names_;
      instance<bind_member> bind_id_member_;

      instance<init_image_base> init_image_base_;
      traversal::inherits init_image_base_inherits_;
      instance<init_image_member> init_image_member_;
      traversal::names init_image_member_names_;

      instance<init_image_member> init_id_image_member_;

      instance<init_value_base> init_value_base_;
      traversal::inherits init_value_base_inherits_;
      instance<init_value_member> init_value_member_;
      traversal::names init_value_member_names_;
      instance<init_value_member> init_id_value_member_;

      schema_emitter schema_emitter_;
      instance<schema::class_drop> schema_drop_;
      instance<schema::class_create> schema_create_;
    };

    struct include: virtual context
    {
      typedef include base;

      virtual void
      generate ()
      {
        os << "#include <odb/cache-traits.hxx>" << endl;

        if (embedded_schema)
          os << "#include <odb/schema-catalog-impl.hxx>" << endl;

        os << "#include <odb/details/unused.hxx>" << endl;

        if (options.generate_query ())
          os << "#include <odb/details/shared-ptr.hxx>" << endl;

        os << endl;

        extra_pre ();

        os << "#include <odb/" << db << "/traits.hxx>" << endl
           << "#include <odb/" << db << "/database.hxx>" << endl
           << "#include <odb/" << db << "/transaction.hxx>" << endl
           << "#include <odb/" << db << "/connection.hxx>" << endl
           << "#include <odb/" << db << "/statement.hxx>" << endl
           << "#include <odb/" << db << "/statement-cache.hxx>" << endl
           << "#include <odb/" << db << "/object-statements.hxx>" << endl
           << "#include <odb/" << db << "/container-statements.hxx>" << endl
           << "#include <odb/" << db << "/exceptions.hxx>" << endl;

        if (options.generate_query ())
          os << "#include <odb/" << db << "/result.hxx>" << endl;

        extra_post ();

        os << endl;
      }

      virtual void
      extra_pre ()
      {
      }

      virtual void
      extra_post ()
      {
      }
    };
  }
}

#endif // ODB_RELATIONAL_SOURCE_HXX
