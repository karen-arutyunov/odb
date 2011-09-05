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
    // Query parameter generator. A new instance is created for each
    // query, so the customized version can have a counter to implement,
    // for example, numbered parameters (e.g., $1, $2, etc). The auto_id()
    // function is called instead of next() for the automatically-assigned
    // object id member when generating the persist statement.
    //
    struct query_parameters: virtual context
    {
      typedef query_parameters base;

      virtual string
      next ()
      {
        return "?";
      }

      virtual string
      auto_id ()
      {
        return next ();
      }
    };

    struct object_columns: object_columns_base, virtual context
    {
      typedef object_columns base;

      object_columns (bool out,
                      bool last = true,
                      query_parameters* param = 0)
          : out_ (out), param_ (param), last_ (last)
      {
      }

      object_columns (std::string const& table_qname,
                      bool out,
                      bool last = true)
          : out_ (out), param_ (0), table_name_ (table_qname), last_ (last)
      {
      }

      virtual bool
      traverse_column (semantics::data_member& m,
                       string const& name,
                       bool first)
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

          if (container_wrapper (im->type ()))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name.
            //
            column (
              *im, "id",
              table_name_.empty ()
              ? table_name_
              : table_qname (*im, table_prefix (table_name (*c) + "_", 1)),
              column_qname (*im, "id", "object_id"));

          }
          else
          {
            semantics::data_member& id (*id_member (*c));
            column (id, "",
                    table_name_.empty () ? table_name_ : table_qname (*c),
                    column_qname (id));
          }
        }
        else
          column (m, "", table_name_, quote_id (name));

        if (param_ != 0)
        {
          line_ += '=';
          line_ += param_->next ();
        }

        return true;
      }

      virtual void
      column (semantics::data_member&,
              string const& /*key_prefix*/,
              string const& table,
              string const& column)
      {
        if (!table.empty ())
        {
          line_ += table;
          line_ += '.';
        }

        line_ += column; // Already quoted.
      }

      virtual void
      flush ()
      {
        if (!last_)
          line_ += ',';

        if (!line_.empty ())
          os << strlit (line_);

        os << endl;
      }

    protected:
      bool out_;
      query_parameters* param_;
      string line_;
      string table_name_;

    private:
      bool last_;
    };

    struct object_joins: object_columns_base, virtual context
    {
      typedef object_joins base;

      //@@ context::{cur,top}_object Might have to be created every time.
      //
      object_joins (semantics::class_& scope, bool query)
          : query_ (query),
            table_ (table_qname (scope)),
            id_ (*id_member (scope))
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

          // If this is a self-join, alias it as '_' to resolve any
          // ambiguities.
          //
          if (i->table == table_)
            line += " AS _";

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
      traverse_column (semantics::data_member& m,
                       string const& col_name,
                       bool)
      {
        semantics::class_* c (object_pointer (m.type ()));

        if (c == 0)
          return true;

        string t, dt;
        std::ostringstream cond, dcond; // @@ diversion?

        if (semantics::data_member* im = inverse (m))
        {
          if (container_wrapper (im->type ()))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name.
            //
            string const& ct (table_name (*c));
            table_prefix tp (ct + "_", 1);
            t = table_qname (*im, tp);
            string const& val (column_qname (*im, "value", "value"));

            cond << t << '.' << val << " = " <<
              table_ << "." << column_qname (id_);

            // Add the join for the object itself so that we are able to
            // use it in the WHERE clause.
            //
            if (query_)
            {
              dt = quote_id (ct);
              string const& id (column_qname (*im, "id", "object_id"));

              // If this is a self-join, use the '_' alias instead of the
              // table name.
              //
              if (dt == table_)
                dcond << "_";
              else
                dcond << dt;

              dcond << '.' << column_qname (*id_member (*c)) << " = " <<
                t << '.' << id;
            }
          }
          else
          {
            t = table_qname (*c);

            // If this is a self-join, use the '_' alias instead of the
            // table name.
            //
            if (t == table_)
              cond << "_";
            else
              cond << t;

            cond << '.' << column_qname (*im) << " = " <<
              table_ << "." << column_qname (id_);
          }
        }
        else if (query_)
        {
          // We need the join to be able to use the referenced object
          // in the WHERE clause.
          //
          t = table_qname (*c);

          // If this is a self-join, use the '_' alias instead of the
          // table name.
          //
          if (t == table_)
            cond << "_";
          else
            cond << t;

          cond << '.' << column_qname (*id_member (*c)) << " = " <<
            table_ << "." << quote_id (col_name);
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
      string table_; //@@ No longer used because of the _ alias.
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
        bool obj (object (c));

        // Ignore transient bases. Not used for views.
        //
        if (!(obj || composite (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl;

        if (obj)
          os << "object_traits< " << c.fq_name () <<
            " >::bind (b + n, i, out);";
        else
          os << "composite_value_traits< " << c.fq_name () <<
            " >::bind (b + n, i);";

        os << "n += " << in_column_count (c) << "UL;"
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
        bool obj (object (c));

        // Ignore transient bases. Not used for views.
        //
        if (!(obj || composite (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl;

        os << "if (" << (obj ? "object" : "composite_value") << "_traits< " <<
          c.fq_name () << " >::grow (i, t + " << index_ << "UL))" << endl
           << "grew = true;"
           << endl;

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
        bool obj (object (c));

        // Ignore transient bases. Not used for views.
        //
        if (!(obj || composite (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << "if (" << (obj ? "object" : "composite_value") << "_traits< " <<
          c.fq_name () << " >::init (i, o))" << endl
           << "grew = true;"
           << endl;
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
        bool obj (object (c));

        // Ignore transient bases. Not used for views.
        //
        if (!(obj || composite (c)))
          return;

        os << "// " << c.name () << " base" << endl
           << "//" << endl
           << (obj ? "object" : "composite_value") << "_traits< " <<
          c.fq_name () << " >::init (o, i, db);"
           << endl;
      }
    };

    // Member-specific traits types for container members.
    //
    struct container_traits: object_members_base, virtual context
    {
      typedef container_traits base;

      container_traits (semantics::class_& c)
          : object_members_base (true, true), c_ (c)
      {
        if (object (c))
          scope_ = "access::object_traits< " + c.fq_name () + " >";
        else
          scope_ = "access::composite_value_traits< " + c.fq_name () + " >";
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
      traverse_composite (semantics::data_member* m, semantics::class_& c)
      {
        if (object (c_))
          object_members_base::traverse_composite (m, c);
        else
        {
          // If we are generating traits for a composite value type, then
          // we don't want to go into its bases or it composite members.
          //
          if (m == 0 && &c == &c_)
            names (c);
        }
      }

      virtual void
      container_extra (semantics::data_member&, semantics::type&)
      {
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type& t)
      {
        using semantics::type;

        // Figure out if this member is from a base object or composite
        // value and whether it is abstract.
        //
        bool base, abst;

        if (object (c_))
        {
          base = cur_object != &c_ ||
            !object (dynamic_cast<type&> (m.scope ()));
          abst = abstract (c_);
        }
        else
        {
          base = false; // We don't go into bases.
          abst = true;  // Always abstract.
        }

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

        bool eager_ptr (is_a (m, test_eager_pointer, vt, "value"));

        if (!eager_ptr)
        {
          if (semantics::class_* cvt = composite_wrapper (vt))
            eager_ptr = has_a (*cvt, test_eager_pointer);
        }

        string name (prefix_ + public_name (m) + "_traits");
        string scope (scope_ + "::" + name);

        os << "// " << m.name () << endl
           << "//" << endl
           << endl;

        container_extra (m, t);

        //
        // Statements.
        //
        if (!abst)
        {
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

            if (container_wrapper (im->type ()))
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
              inv_id = column_qname (*id_member (*c));
              inv_fid = column_qname (*im);
            }

            instance<query_parameters> qp;

            os << strlit ("SELECT ") << endl
               << strlit (inv_table + "." + inv_fid + ',') << endl
               << strlit (inv_table + "." + inv_id) << endl
               << strlit (" FROM " + inv_table +
                          " WHERE " + inv_table + "." + inv_fid + "=" +
                          qp->next ());
          }
          else
          {
            string const& id_col (column_qname (m, "id", "object_id"));

            os << strlit ("SELECT ") << endl
               << strlit (table + "." + id_col + ',') << endl;

            switch (ck)
            {
            case ck_ordered:
              {
                if (ordered)
                {
                  instance<object_columns> t (table, false, false);
                  string const& col (column_qname (m, "index", "index"));
                  t->column (m, "index", table, col);
                  t->flush ();
                }
                break;
              }
            case ck_map:
            case ck_multimap:
              {
                instance<object_columns> t (table, false, false);

                if (semantics::class_* ckt = composite_wrapper (*kt))
                  t->traverse (m, *ckt, "key", "key");
                else
                {
                  string const& col (column_qname (m, "key", "key"));
                  t->column (m, "key", table, col);
                  t->flush ();
                }
                break;
              }
            case ck_set:
            case ck_multiset:
              {
                break;
              }
            }

            instance<object_columns> t (table, false);

            if (semantics::class_* cvt = composite_wrapper (vt))
              t->traverse (m, *cvt, "value", "value");
            else
            {
              string const& col (column_qname (m, "value", "value"));
              t->column (m, "value", table, col);
              t->flush ();
            }

            instance<query_parameters> qp;

            os << strlit (" FROM " + table +
                          " WHERE " + table + "." + id_col + "=" +
                          qp->next ());

            if (ordered)
            {
              string const& col (column_qname (m, "index", "index"));

              os << endl
                 << strlit (" ORDER BY " + table + "." + col) << endl;
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
                {
                  instance<object_columns> t (false, false);
                  t->column (m, "index", "", column_qname (m, "index", "index"));
                  t->flush ();
                }
                break;
              }
            case ck_map:
            case ck_multimap:
              {
                instance<object_columns> t (false, false);

                if (semantics::class_* ckt = composite_wrapper (*kt))
                  t->traverse (m, *ckt, "key", "key");
                else
                {
                  t->column (m, "key", "", column_qname (m, "key", "key"));
                  t->flush ();
                }
                break;
              }
            case ck_set:
            case ck_multiset:
              {
                break;
              }
            }

            instance<object_columns> t (false);

            if (semantics::class_* cvt = composite_wrapper (vt))
              t->traverse (m, *cvt, "value", "value");
            else
            {
              t->column (m, "value", "", column_qname (m, "value", "value"));
              t->flush ();
            }

            string values;
            instance<query_parameters> qp;
            for (size_t i (0), n (m.get<size_t> ("data-column-count"));
                 i < n; ++i)
            {
              if (i != 0)
                values += ',';

              values += qp->next ();
            }

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
            instance<query_parameters> qp;

            os << strlit ("DELETE FROM " + table) << endl
               << strlit (" WHERE " + column_qname (m, "id", "object_id") +
                          "=" + qp->next ()) << ";"
               << endl;
          }
        }

        if (base)
          return;

        //
        // Functions.
        //

        // bind()
        //
        {
          // bind (cond_image_type)
          //
          os << "void " << scope << "::" << endl
             << "bind (" << bind_vector << " b," << endl
             << "const " << bind_vector << " id," << endl
             << "std::size_t id_size," << endl
             << "cond_image_type& c)"
             << "{"
             << "ODB_POTENTIALLY_UNUSED (c);"
             << endl
             << "std::size_t n (0);"
             << endl;

          os << "// object_id" << endl
             << "//" << endl
             << "if (id != 0)" << endl
             << "std::memcpy (&b[n], id, id_size * sizeof (id[0]));"
             << "n += id_size;"
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
             << "bind (" << bind_vector << " b," << endl
             << "const " << bind_vector << " id," << endl
             << "std::size_t id_size," << endl
             << "data_image_type& d)"
             << "{"
             << "size_t n (0);"
             << endl;

          os << "// object_id" << endl
             << "//" << endl
             << "if (id != 0)" << endl
             << "std::memcpy (&b[n], id, id_size * sizeof (id[0]));"
             << "n += id_size;"
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

              if (semantics::class_* c = composite_wrapper (*kt))
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
               << "statements_type& sts (*static_cast< statements_type* > (d));"
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
               << "if (di.version != sts.data_image_version ())"
               << "{"
               << "bind (b.bind, 0, sts.id_binding ().count, di);"
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
           << "statements_type& sts (*static_cast< statements_type* > (d));"
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
          os << "const binding& idb (sts.id_binding ());"
             << endl
             << "if (di.version != sts.data_image_version () ||" << endl
             << "idb.version != sts.data_id_binding_version ())"
             << "{"
             << "binding& b (sts.data_image_binding ());"
             << "bind (b.bind, idb.bind, idb.count, di);"
             << "sts.data_image_version (di.version);"
             << "sts.data_id_binding_version (idb.version);"
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
             << "bind (b.bind, 0, sts.id_binding ().count, di);"
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
             << "statements_type& sts (*static_cast< statements_type* > (d));"
             << "sts.delete_all_statement ().execute ();";

        os << "}";

        // persist
        //
        if (!inverse)
        {
          os << "void " << scope << "::" << endl
             << "persist (const container_type& c," << endl
             << "const " << db << "::binding& id," << endl
             << "statements_type& sts)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << "binding& b (sts.data_image_binding ());"
             << "if (id.version != sts.data_id_binding_version () || " <<
            "b.version == 0)"
             << "{"
             << "bind (b.bind, id.bind, id.count, sts.data_image ());"
             << "sts.data_id_binding_version (id.version);"
             << "b.version++;"
             << "}"
             << "sts.id_binding (id);"
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
           << "const " << db << "::binding& id," << endl
           << "statements_type& sts)"
           << "{"
           << "using namespace " << db << ";"
           << endl
           << "binding& db (sts.data_image_binding ());"
           << "if (id.version != sts.data_id_binding_version () || " <<
          "db.version == 0)"
           << "{"
           << "bind (db.bind, id.bind, id.count, sts.data_image ());"
           << "sts.data_id_binding_version (id.version);"
           << "db.version++;"
           << "}"
           << "binding& cb (sts.cond_image_binding ());"
           << "if (id.version != sts.cond_id_binding_version () || " <<
          "cb.version == 0)"
           << "{"
           << "bind (cb.bind, id.bind, id.count, sts.cond_image ());"
           << "sts.cond_id_binding_version (id.version);"
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
             << "bind (db.bind, 0, id.count, sts.data_image ());"
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
           << "sts.id_binding (id);"
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
             << "const " << db << "::binding& id," << endl
             << "statements_type& sts)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << "binding& db (sts.data_image_binding ());"
             << "if (id.version != sts.data_id_binding_version () || " <<
            "db.version == 0)"
             << "{"
             << "bind (db.bind, id.bind, id.count, sts.data_image ());"
             << "sts.data_id_binding_version (id.version);"
             << "db.version++;"
             << "}"
            //
            // We may need cond if the specialization calls delete_all.
            //
             << "binding& cb (sts.cond_image_binding ());"
             << "if (id.version != sts.cond_id_binding_version () || " <<
            "cb.version == 0)"
             << "{"
             << "bind (cb.bind, id.bind, id.count, sts.cond_image ());"
             << "sts.cond_id_binding_version (id.version);"
             << "cb.version++;"
             << "}"
             << "sts.id_binding (id);"
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
             << "erase (const " << db << "::binding& id, statements_type& sts)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << "binding& b (sts.cond_image_binding ());"
             << "if (id.version != sts.cond_id_binding_version () || " <<
            "b.version == 0)"
             << "{"
             << "bind (b.bind, id.bind, id.count, sts.cond_image ());"
             << "sts.cond_id_binding_version (id.version);"
             << "b.version++;"
             << "}"
             << "sts.id_binding (id);"
             << "functions_type& fs (sts.functions ());";

          if (ck == ck_ordered)
            os << "fs.ordered (" << (ordered ? "true" : "false") << ");";

          os << "container_traits::erase (fs);"
             << "}";
        }
      }

    protected:
      string scope_;
      semantics::class_& c_;
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
      traverse_container (semantics::data_member& m, semantics::type&)
      {
        string traits (prefix_ + public_name (m) + "_traits");
        os << db << "::container_statements_impl< " << traits << " > " <<
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
      traverse_container (semantics::data_member& m, semantics::type&)
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
          : object_members_base (true, false),
            call_ (call),
            obj_prefix_ ("obj.")
      {
      }

      virtual void
      traverse_composite_wrapper (semantics::data_member* m,
                                  semantics::class_& c,
                                  semantics::type* w)
      {
        if (m == 0)
        {
          object_members_base::traverse_composite (m, c);
          return;
        }

        string old (obj_prefix_);
        obj_prefix_ += m->name ();

        // If this is a wrapped composite value, then we need to
        // "unwrap" it.
        //
        if (w != 0)
        {
          // Because we cannot have nested containers, m.type () should
          // be the same as w.
          //
          assert (m != 0 && &m->type () == w);
          string const& type (m->type ().fq_name (m->belongs ().hint ()));

          obj_prefix_ = "wrapper_traits< " + type + " >::" +
            (call_ == load_call ? "set_ref" : "get_ref") +
            " (" + obj_prefix_ + ")";
        }

        obj_prefix_ += '.';

        object_members_base::traverse_composite (m, c);
        obj_prefix_ = old;
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type&)
      {
        using semantics::type;

        bool inverse (context::inverse (m, "value"));

        string const& name (m.name ());
        string obj_name (obj_prefix_ + name);
        string sts_name (prefix_ + name);
        string traits (prefix_ + public_name (m) + "_traits");

        // If this is a wrapped container, then we need to "unwrap" it.
        //
        {
          semantics::type& t (m.type ());
          if (wrapper (t))
          {
            string const& type (t.fq_name (m.belongs ().hint ()));

            obj_name = "wrapper_traits< " + type + " >::" +
              (call_ == load_call ? "set_ref" : "get_ref") +
              " (" + obj_name + ")";
          }
        }

        switch (call_)
        {
        case persist_call:
          {
            if (!inverse)
              os << traits << "::persist (" << endl
                 << obj_name << "," << endl
                 << "idb," << endl
                 << "sts.container_statment_cache ()." << sts_name << ");"
                 << endl;
            break;
          }
        case load_call:
          {
            os << traits << "::load (" << endl
               << obj_name << "," << endl
               << "idb," << endl
               << "sts.container_statment_cache ()." << sts_name << ");"
               << endl;
            break;
          }
        case update_call:
          {
            if (!inverse)
              os << traits << "::update (" << endl
                 << obj_name << "," << endl
                 << "idb," << endl
                 << "sts.container_statment_cache ()." << sts_name << ");"
                 << endl;
            break;
          }
        case erase_call:
          {
            if (!inverse)
              os << traits << "::erase (" << endl
                 << "idb," << endl
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

    // Output a list of parameters for the persist statement.
    //
    struct persist_statement_params: object_members_base, virtual context
    {
      persist_statement_params (string& params)
          : params_ (params), count_ (0)
      {
      }

      virtual void
      traverse_simple (semantics::data_member& m)
      {
        if (!inverse (m))
        {
          if (count_++ != 0)
            params_ += ',';

          if (m.count ("id") && m.count ("auto"))
            params_ += qp->auto_id ();
          else
            params_ += qp->next ();
        }
      }

    private:
      string& params_;
      size_t count_;
      instance<query_parameters> qp;
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

        if (object (c))
          traverse_object (c);
        else if (view (c))
          traverse_view (c);
        else if (composite (c))
          traverse_composite (c);
      }

      //
      // common
      //

      virtual void
      post_query_ (type&)
      {
      }

      //
      // object
      //

      virtual void
      object_extra (type&)
      {
      }

      virtual void
      object_query_statement_ctor_args (type&)
      {
        os << "sts.connection ()," << endl
           << "query_clause + q.clause (table_name)," << endl
           << "q.parameters_binding ()," << endl
           << "imb";
      }

      virtual void
      object_erase_query_statement_ctor_args (type&)
      {
        os << "conn," << endl
           << "erase_query_clause + q.clause (table_name)," << endl
           << "q.parameters_binding ()";
      }

      virtual void
      traverse_object (type& c)
      {
        bool abst (abstract (c));
        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        bool grow (context::grow (c));

        semantics::data_member* id (id_member (c));
        bool auto_id (id ? id->count ("auto") : false);
        bool grow_id (id ? context::grow (*id) : false);
        bool base_id (id ? &id->scope () != &c : false); // Comes from base.

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        object_extra (c);

        //
        // Query.
        //

        if (options.generate_query ())
        {
          instance<query_columns> t (c);
          t->traverse (c);
        }

        //
        // Containers (abstract and concrete).
        //
        bool straight_containers (has_a (c, test_straight_container));
        bool containers (straight_containers || has_a (c, test_container));

        if (containers)
        {
          instance<container_traits> t (c);
          t->traverse (c);
        }

        //
        // Functions (abstract and concrete).
        //

        // id (image_type)
        //
        if (id != 0 && options.generate_query () && !base_id)
        {
          os << traits << "::id_type" << endl
             << traits << "::" << endl
             << "id (const image_type& i)"
             << "{"
             << "id_type id;";
          init_id_value_member_->traverse (*id);
          os << "return id;"
             << "}";
        }

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
        if (id != 0 && !base_id)
        {
          os << "void " << traits << "::" << endl
             << "bind (" << bind_vector << " b, id_image_type& i)"
             << "{"
             << "std::size_t n (0);";
          bind_id_member_->traverse (*id);
          os << "}";
        }

        // init (image, object)
        //
        os << "bool " << traits << "::" << endl
           << "init (image_type& i, const object_type& o)"
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
           << "init (object_type& o, const image_type& i, database& db)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << endl;

        inherits (c, init_value_base_inherits_);
        names (c, init_value_member_names_);

        os << "}";

        // init (id_image, id)
        //
        if (id != 0 && !base_id)
        {
          os << "void " << traits << "::" << endl
             << "init (id_image_type& i, const id_type& id)"
             << "{";

          if (grow_id)
            os << "bool grew (false);";

          init_id_image_member_->traverse (*id);

          if (grow_id)
            os << endl
               << "if (grew)" << endl
               << "i.version++;";

          os << "}";
        }

        //
        // The rest only applies to concrete objects.
        //
        if (abst)
          return;

        //
        // Containers (concrete).
        //

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

        //
        // Statements.
        //

        string const& table (table_qname (c));
        string const& id_col (column_qname (*id));

        // persist_statement
        //
        {
          os << "const char* const " << traits << "::persist_statement " <<
            "=" << endl
             << strlit ("INSERT INTO " + table + " (") << endl;

          instance<object_columns> ct (false);
          ct->traverse (c);

          string values;
          instance<persist_statement_params> pt (values);
          pt->traverse (c);

          os << strlit (") VALUES (" + values + ")") << ";"
             << endl;
        }

        // find_statement
        //
        {
          os << "const char* const " << traits << "::find_statement =" << endl
             << strlit ("SELECT ") << endl;

          instance<object_columns> t (table, true);
          t->traverse (c);

          os << strlit (" FROM " + table) << endl;

          bool f (false);
          instance<object_joins> j (c, f); // @@ (im)perfect forwarding
          j->traverse (c);
          j->write ();

          instance<query_parameters> qp;
          os << strlit (" WHERE " + table + "." + id_col + "=" +
                        qp->next ()) << ";"
             << endl;
        }

        // update_statement
        //
        {
          os << "const char* const " << traits << "::update_statement " <<
            "=" << endl
             << strlit ("UPDATE " + table + " SET ") << endl;

          instance<query_parameters> qp;
          instance<object_columns> t (false, true, qp.get ());
          t->traverse (c);

          os << strlit (" WHERE " + id_col + "=" + qp->next ()) << ";"
             << endl;
        }

        // erase_statement
        //
        {
          instance<query_parameters> qp;
          os << "const char* const " << traits << "::erase_statement =" << endl
             << strlit ("DELETE FROM " + table) << endl
             << strlit (" WHERE " + id_col + "=" + qp->next ()) << ";"
             << endl;
        }

        if (options.generate_query ())
        {
          // query_clause
          //
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

          // erase_query_clause
          //
          os << "const char* const " << traits << "::erase_query_clause =" << endl
            << strlit ("DELETE FROM " + table) << endl;

          // DELETE JOIN:
          //
          // MySQL:
          // << strlit ("DELETE FROM " + table + " USING " + table) << endl;
          // << strlit ("DELETE " + table + " FROM " + table) << endl;
          // oj->write ();
          //

          os << strlit (" ") << ";"
             << endl;

          // table_name
          //
          os << "const char* const " << traits << "::table_name =" << endl
             << strlit (table) << ";"
             << endl;
        }

        // persist ()
        //
        os << "void " << traits << "::" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type& obj)"
           << "{"
           << "using namespace " << db << ";"
           << endl
           << db << "::connection& conn (" << endl
           << db << "::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find_object<object_type> ());"
           << "image_type& im (sts.image ());"
           << "binding& imb (sts.in_image_binding ());"
           << endl
           << "if (init (im, obj))" << endl
           << "im.version++;"
           << endl;

        if (auto_id)
        {
          string const& n (id->name ());
          string var ("im." + n + (n[n.size () - 1] == '_' ? "" : "_"));
          init_auto_id (*id, var);
          os << endl;
        }

        os << "if (im.version != sts.in_image_version () || imb.version == 0)"
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
          os << "obj." << id->name () << " = static_cast<id_type> (st.id ());"
             << endl;

        if (straight_containers)
        {
          // Initialize id_image and binding.
          //
          os << "id_image_type& i (sts.id_image ());"
             << "init (i, obj." << id->name () << ");"
             << endl
             << "binding& idb (sts.id_image_binding ());"
             << "if (i.version != sts.id_image_version () || idb.version == 0)"
             << "{"
             << "bind (idb.bind, i);"
             << "sts.id_image_version (i.version);"
             << "idb.version++;"
             << "}";

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
           << db << "::connection& conn (" << endl
           << db << "::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find_object<object_type> ());"
           << endl;

        // Initialize id image.
        //
        os << "id_image_type& i (sts.id_image ());"
           << "init (i, obj." << id->name () << ");"
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
           << endl
           << "if (init (im, obj))" << endl
           << "im.version++;"
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
           << db << "::connection& conn (" << endl
           << db << "::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find_object<object_type> ());"
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

        // Erase containers first so that there are no reference
        // violations (we don't want to reply on ON DELETE CASCADE
        // here since in case of a custom schema, it might not be
        // there).
        //
        if (straight_containers)
        {
          instance<container_calls> t (container_calls::erase_call);
          t->traverse (c);
          os << endl;
        }

        os << "if (sts.erase_statement ().execute () != 1)" << endl
           << "throw object_not_persistent ();";

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
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
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
             << "callback (db, obj, callback_event::pre_load);"
             << "init (obj, sts.image (), db);"
             << "load_ (sts, obj);"
             << "sts.load_delayed ();"
             << "l.unlock ();"
             << "callback (db, obj, callback_event::post_load);"
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
           << db << "::connection& conn (" << endl
           << db << "::transaction::current ().connection ());"
           << "object_statements< object_type >& sts (" << endl
           << "conn.statement_cache ().find_object<object_type> ());"
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
           << "callback (db, obj, callback_event::pre_load);"
           << "init (obj, sts.image (), db);"
           << "load_ (sts, obj);"
           << "sts.load_delayed ();"
           << "l.unlock ();"
           << "callback (db, obj, callback_event::post_load);"
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
             << "if (grow (im, sts.out_image_truncated ()))" << endl
             << "im.version++;"
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
             << db << "::binding& idb (sts.id_image_binding ());"
             << endl;
          instance<container_calls> t (container_calls::load_call);
          t->traverse (c);
          os << "}";
        }

        if (options.generate_query ())
        {
          // query ()
          //
          os << "template<>" << endl
             << "result< " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query< " << traits << "::object_type > (" << endl
             << "database& db," << endl
             << "const query_type& q)"
             << "{"
             << "using namespace " << db << ";"
             << "using odb::details::shared;"
             << "using odb::details::shared_ptr;"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << endl
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << "shared_ptr<select_statement> st;"
             << endl
             << "query_ (db, q, sts, st);"
             << endl
             << "shared_ptr<odb::result_impl<object_type, " <<
            "class_object> > r (" << endl
             << "new (shared) " << db <<
            "::result_impl<object_type, class_object> (" << endl
             << "q, st, sts));"
             << endl
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
             << "using odb::details::shared;"
             << "using odb::details::shared_ptr;"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << endl
             << "object_statements< object_type >& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << "shared_ptr<select_statement> st;"
             << endl
             << "query_ (db, q, sts, st);"
             << endl
             << "shared_ptr<odb::result_impl<" <<
            "const object_type, class_object> > r (" << endl
             << "new (shared) " << db <<
            "::result_impl<const object_type, class_object> (" << endl
             << "q, st, sts));"
             << endl
             << "return result<const object_type> (r);"
             << "}";

          os << "void " << traits << "::" << endl
             << "query_ (database&," << endl
             << "const query_type& q," << endl
             << db << "::object_statements< object_type >& sts," << endl
             << "odb::details::shared_ptr<" << db << "::select_statement>& st)"
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
             << "st.reset (new (odb::details::shared) select_statement ("
             << endl;

          object_query_statement_ctor_args (c);

          os << "));" << endl
             << "st->execute ();";

          post_query_ (c);

          os << "}";

          // erase_query
          //
          os << "unsigned long long " << traits << "::" << endl
             << "erase_query (database&, const query_type& q)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << endl
             << "delete_statement st (" << endl;

          object_erase_query_statement_ctor_args (c);

          os << ");"
             << endl
             << "return st.execute ();"
             << "}";
        }

        if (embedded_schema)
          schema (c);
      }

      struct schema_emitter: emitter, virtual context
      {
        void
        pass (unsigned short p)
        {
          empty_ = true;
          pass_ = p;
          new_pass_ = true;
        }

        // Did this pass produce anything?
        //
        bool
        empty () const
        {
          return empty_;
        }

        virtual void
        pre ()
        {
          first_ = true;
        }

        virtual void
        line (const string& l)
        {
          if (first_)
          {
            first_ = false;

            // If this line starts a new pass, then output the
            // switch/case blocks.
            //
            if (new_pass_)
            {
              new_pass_ = false;
              empty_ = false;

              if (pass_ == 1)
              {
                os << "switch (pass)"
                   << "{"
                   << "case 1:" << endl
                   << "{";
              }
              else
              {
                os << "return true;" // One more pass.
                   << "}"
                   << "case " << pass_ << ":" << endl
                   << "{";
              }
            }

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
        bool empty_;
        bool new_pass_;
        unsigned short pass_;
      };

      virtual void
      schema (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        // create_schema ()
        //
        os << "bool " << traits << "::" << endl
           << "create_schema (database& db, unsigned short pass, bool drop)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << "ODB_POTENTIALLY_UNUSED (pass);"
           << endl;

        // Drop.
        //
        {
          bool close (false);

          os << "if (drop)"
             << "{";

          for (unsigned short pass (1); pass < 3; ++pass)
          {
            schema_emitter_.pass (pass);
            schema_drop_->pass (pass);
            schema_drop_->traverse (c);
            close = close || !schema_emitter_.empty ();
          }

          if (close) // Close the last case and the switch block.
            os << "return false;"
               << "}"  // case
               << "}";  // switch

          os << "}";
        }

        // Create.
        //
        {
          bool close (false);

          os << "else"
             << "{";

          for (unsigned short pass (1); pass < 3; ++pass)
          {
            schema_emitter_.pass (pass);
            schema_create_->pass (pass);
            schema_create_->traverse (c);
            close = close || !schema_emitter_.empty ();
          }

          if (close) // Close the last case and the switch block.
            os << "return false;"
               << "}"  // case
               << "}"; // switch

          os << "}";
        }

        os << "return false;"
           << "}";

        os << "static const schema_catalog_entry" << endl
           << "schema_catalog_entry_" << flat_name (type) << "_ (" << endl
           << strlit (options.default_schema ()) << "," << endl
           << "&" << traits << "::create_schema);"
           << endl;
      }

      //
      // view
      //

      virtual void
      view_extra (type&)
      {
      }

      virtual void
      view_query_statement_ctor_args (type&)
      {
        os << "sts.connection ()," << endl
           << "query_statement + q.clause (\"\")," << endl
           << "q.parameters_binding ()," << endl
           << "imb";
      }

      virtual void
      traverse_view (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::view_traits< " + type + " >");

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        view_extra (c);

        //
        // Functions.
        //

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
        names (c, grow_member_names_);

        os << "return grew;"
           << "}";

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (" << bind_vector << " b, image_type& i)"
           << "{"
           << "bool out (true);" //@@ Try to get rid of this.
           << "ODB_POTENTIALLY_UNUSED (out);"
           << endl
           << "std::size_t n (0);"
           << endl;

        names (c, bind_member_names_);

        os << "}";

        // init (view, image)
        //
        os << "void " << traits << "::" << endl
           << "init (view_type& o, const image_type& i)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << endl;

        names (c, init_value_member_names_);

        os << "}";

        // query_statement
        //
        os << "const char* const " << traits << "::query_statement =" << endl
           << strlit (c.get<string> ("query")) << endl
           << strlit (" ") << ";" << endl
           << endl;

        // query ()
        //
        os << "result< " << traits << "::view_type >" << endl
           << traits << "::" << endl
           << "query (database&, const query_type& q)"
           << "{"
           << "using namespace " << db << ";"
           << "using odb::details::shared;"
           << "using odb::details::shared_ptr;"
           << endl
           << db << "::connection& conn (" << endl
           << db << "::transaction::current ().connection ());"
           << endl
           << "view_statements< view_type >& sts (" << endl
           << "conn.statement_cache ().find_view<view_type> ());"
           << endl
           << "image_type& im (sts.image ());"
           << "binding& imb (sts.image_binding ());"
           << endl
           << "if (im.version != sts.image_version () || imb.version == 0)"
           << "{"
           << "bind (imb.bind, im);"
           << "sts.image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "shared_ptr<select_statement> st (" << endl
           << "new (shared) select_statement (" << endl;

        view_query_statement_ctor_args (c);

        os << "));" << endl
           << "st->execute ();";

        post_query_ (c);

        os << endl
           << "shared_ptr<odb::result_impl<view_type, " <<
          "class_view> > r (" << endl
           << "new (shared) " << db <<
          "::result_impl<view_type, class_view> (" << endl
           << "q, st, sts));"
           << endl
           << "return result<view_type> (r);"
           << "}";
      }

      //
      // composite
      //

      virtual void
      traverse_composite (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::composite_value_traits< " + type + " >");

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        // Containers.
        //
        {
          instance<container_traits> t (c);
          t->traverse (c);
        }

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

        // init (image, value)
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

        // init (value, image)
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
        extra_pre ();

        os << "#include <cstring> // std::memcpy" << endl
           << endl;

        os << "#include <odb/cache-traits.hxx>" << endl;

        if (embedded_schema)
          os << "#include <odb/schema-catalog-impl.hxx>" << endl;

        if (options.generate_query ())
          os << "#include <odb/details/shared-ptr.hxx>" << endl;

        os << endl;

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
