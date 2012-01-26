// file      : odb/relational/source.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SOURCE_HXX
#define ODB_RELATIONAL_SOURCE_HXX

#include <map>
#include <set>
#include <list>
#include <vector>
#include <sstream>

#include <odb/diagnostics.hxx>
#include <odb/emitter.hxx>

#include <odb/relational/context.hxx>
#include <odb/relational/common.hxx>
#include <odb/relational/schema.hxx>

namespace relational
{
  namespace source
  {
    // Column literal in a statement (e.g., in select-list, etc).
    //
    struct statement_column
    {
      statement_column (): member (0) {}
      statement_column (std::string const& c,
                        semantics::data_member& m,
                        std::string const& kp = "")
          : column (c), member (&m), key_prefix (kp)
      {
      }

      std::string column;
      semantics::data_member* member;
      std::string key_prefix;
    };

    typedef std::list<statement_column> statement_columns;

    // Query parameter generator. A new instance is created for each
    // query, so the customized version can have a counter to implement,
    // for example, numbered parameters (e.g., $1, $2, etc). The auto_id()
    // function is called instead of next() for the automatically-assigned
    // object id member when generating the persist statement. If empty
    // string is returned, then parameter is ignored.
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

      object_columns (statement_kind sk,
                      statement_columns& sc,
                      query_parameters* param = 0)
          : sk_ (sk), sc_ (sc), param_ (param)
      {
      }

      object_columns (std::string const& table_qname,
                      statement_kind sk,
                      statement_columns& sc)
          : sk_ (sk), sc_ (sc), param_ (0), table_name_ (table_qname)
      {
      }

      virtual bool
      traverse_column (semantics::data_member& m, string const& name, bool)
      {
        semantics::data_member* im (inverse (m));

        // Ignore certain columns depending on what kind statement we are
        // generating. Columns corresponding to the inverse members are
        // only present in the select statements while the id and readonly
        // columns are not present in the update statements.
        //
        if (im != 0 && sk_ != statement_select)
          return false;

        if ((id (m) || readonly (member_path_, member_scope_)) &&
            sk_ == statement_update)
          return false;

        // Inverse object pointers come from a joined table.
        //
        if (im != 0)
        {
          semantics::class_* c (object_pointer (utype (m)));

          if (container (*im))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name. We don't assign join
            // aliases for container tables so use the actual table name.
            //
            column (
              *im,
              "id",
              table_name_.empty ()
              ? table_name_
              : table_qname (*im, table_prefix (table_name (*c) + "_", 1)),
              column_qname (*im, "id", "object_id"));
          }
          else
          {
            semantics::data_member& id (*id_member (*c));

            // Use the join alias (column name) instead of the actual
            // table name unless we are handling a container.
            //
            column (
              id,
              "",
              table_name_.empty () ? table_name_ : quote_id (name),
              column_qname (id));
          }
        }
        else
          column (m, "", table_name_, quote_id (name));

        return true;
      }

      virtual void
      column (semantics::data_member& m,
              string const& key_prefix,
              string const& table,
              string const& column)
      {
        string r;

        if (!table.empty ())
        {
          r += table; // Already quoted.
          r += '.';
        }

        r += column; // Already quoted.

        // Version column (optimistic concurrency) requires special
        // handling in the UPDATE statement.
        //
        //
        if (sk_ == statement_update && version (m))
        {
          r += "=" + r + "+1";
        }
        else if (param_ != 0)
        {
          r += '=';
          r += param_->next ();
        }

        sc_.push_back (statement_column (r, m, key_prefix));
      }

    protected:
      statement_kind sk_;
      statement_columns& sc_;
      query_parameters* param_;
      string table_name_;
    };

    struct view_columns: object_columns_base, virtual context
    {
      typedef view_columns base;

      view_columns (statement_columns& sc): sc_ (sc), in_composite_ (false) {}

      virtual void
      traverse_composite (semantics::data_member* pm, semantics::class_& c)
      {
        if (in_composite_)
        {
          object_columns_base::traverse_composite (pm, c);
          return;
        }

        // Override the column prerix.
        //
        semantics::data_member& m (*pm);

        // If we have literal column specified, use that.
        //
        if (m.count ("column"))
        {
          table_column const& tc (m.get<table_column> ("column"));

          if (!tc.table.empty ())
            table_prefix_ = tc.table;

          column_prefix_ = object_columns_base::column_prefix (m);
        }
        // Otherwise, see if there is a column expression. For composite
        // members in a view, this should be a single reference.
        //
        else if (m.count ("column-expr"))
        {
          column_expr const& e (m.get<column_expr> ("column-expr"));

          if (e.size () > 1)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column ()
                 << ": error: column expression specified for a data member "
                 << "of a composite value type" << endl;

            throw operation_failed ();
          }

          data_member_path const& mp (e.back ().member_path);

          if (mp.size () > 1)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column ()
                 << ": error: invalid data member in db pragma column"
                 << endl;

            throw operation_failed ();
          }

          table_prefix_ = e.back ().table;
          column_prefix_ = object_columns_base::column_prefix (*mp.back ());
        }
        else
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: no column prefix provided for a view data member"
               << endl;

          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": info: use db pragma column to specify the column prefix"
               << endl;

          throw operation_failed ();
        }

        in_composite_ = true;
        object_columns_base::traverse_composite (pm, c);
        in_composite_ = false;
      }

      virtual bool
      traverse_column (semantics::data_member& m, string const& name, bool)
      {
        string col;

        // If we are inside a composite value, use the standard
        // column name machinery.
        //
        if (in_composite_)
        {
          if (!table_prefix_.empty ())
          {
            col += quote_id (table_prefix_);
            col += '.';
          }

          col += quote_id (name);
        }
        // If we have literal column specified, use that.
        //
        else if (m.count ("column"))
        {
          table_column const& tc (m.get<table_column> ("column"));

          if (!tc.expr)
          {
            if (!tc.table.empty ())
            {
              col += quote_id (tc.table);
              col += '.';
            }

            col += quote_id (tc.column);
          }
          else
            col += tc.column;
        }
        // Otherwise, see if there is a column expression.
        //
        else if (m.count ("column-expr"))
        {
          column_expr const& e (m.get<column_expr> ("column-expr"));

          for (column_expr::const_iterator i (e.begin ()); i != e.end (); ++i)
          {
            switch (i->kind)
            {
            case column_expr_part::literal:
              {
                col += i->value;
                break;
              }
            case column_expr_part::reference:
              {
                col += quote_id (i->table);
                col += '.';
                col += quote_id (column_name (i->member_path));
                break;
              }
            }
          }
        }
        else
        {
          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": error: no column name provided for a view data member"
               << endl;

          cerr << m.file () << ":" << m.line () << ":" << m.column ()
               << ": info: use db pragma column to specify the column name"
               << endl;

          throw operation_failed ();
        }

        column (m, col);
        return true;
      }

      // The column argument is a qualified and quoted column or
      // expression.
      //
      virtual void
      column (semantics::data_member& m, string const& column)
      {
        sc_.push_back (statement_column (column, m));
      }

    protected:
      statement_columns& sc_;
      bool in_composite_;
      qname table_prefix_; // Table corresponding to column_prefix_;
    };

    struct object_joins: object_columns_base, virtual context
    {
      typedef object_joins base;

      //@@ context::{cur,top}_object; might have to be created every time.
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

          if (!i->alias.empty ())
            line += (need_alias_as ? " AS " : " ") + i->alias;

          line += " ON ";
          line += i->cond;

          os << strlit (line) << endl;
        }
      }

      virtual bool
      traverse_column (semantics::data_member& m, string const& column, bool)
      {
        semantics::class_* c (object_pointer (utype (m)));

        if (c == 0)
          return false;

        string t, a, dt, da;
        std::ostringstream cond, dcond; // @@ diversion?

        if (semantics::data_member* im = inverse (m))
        {
          if (container (*im))
          {
            // This container is a direct member of the class so the table
            // prefix is just the class table name.
            //
            qname const& ct (table_name (*c));
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
              da = quote_id (column);

              string const& id (column_qname (*im, "id", "object_id"));

              dcond << da << '.' << column_qname (*id_member (*c)) << " = " <<
                t << '.' << id;
            }
          }
          else
          {
            t = table_qname (*c);
            a = quote_id (column);

            cond << a << '.' << column_qname (*im) << " = " <<
              table_ << "." << column_qname (id_);
          }
        }
        else if (query_)
        {
          // We need the join to be able to use the referenced object
          // in the WHERE clause.
          //
          t = table_qname (*c);
          a = quote_id (column);

          cond << a << '.' << column_qname (*id_member (*c)) << " = " <<
            table_ << "." << quote_id (column);
        }

        if (!t.empty ())
        {
          joins_.push_back (join ());
          joins_.back ().table = t;
          joins_.back ().alias = a;
          joins_.back ().cond = cond.str ();
        }

        // Add dependent join (i.e., an object table join via the
        // container table).
        //
        if (!dt.empty ())
        {
          joins_.push_back (join ());
          joins_.back ().table = dt;
          joins_.back ().alias = da;
          joins_.back ().cond = dcond.str ();
        }

        return true;
      }

    private:
      bool query_;
      string table_;
      semantics::data_member& id_;

      struct join
      {
        string table;
        string alias;
        string cond;
      };

      typedef std::vector<join> joins;

      joins joins_;
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

        os << "// " << class_name (c) << " base" << endl
           << "//" << endl;

        // If the derived class is readonly, then we will never be
        // called with sk == statement_update.
        //
        bool ro (readonly (c));
        bool check (ro && !readonly (*context::top_object));

        if (check)
          os << "if (sk != statement_update)"
             << "{";

        os << (obj ? "object" : "composite_value") << "_traits< " <<
          class_fq_name (c) << " >::bind (b + n, i, sk);";

        column_count_type const& cc (column_count (c));

        os << "n += " << cc.total << "UL";

        // select = total
        // insert = total - inverse - optimistic_managed
        // update = total - inverse - optimistic_managed - id - readonly
        //
        if (cc.inverse != 0 ||
            cc.optimistic_managed != 0 ||
            (!ro && (cc.id != 0 || cc.readonly != 0)))
        {
          os << " - (" << endl
             << "sk == statement_select ? 0 : ";

          if (cc.inverse != 0 || cc.optimistic_managed != 0)
            os << (cc.inverse + cc.optimistic_managed) << "UL";

          if (!ro && (cc.id != 0 || cc.readonly != 0))
          {
            if (cc.inverse != 0 || cc.optimistic_managed != 0)
              os << " + ";

            os << "(" << endl
               << "sk == statement_insert ? ";

            if (insert_send_auto_id || !auto_ (*id_member (c)))
              os << "0";
            else
              os << cc.id << "UL";

            os << " : " << cc.id + cc.readonly << "UL)";
          }

          os << ")";
        }

        os << ";";

        if (check)
          os << "}";
        else
          os << endl;
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

        os << "// " << class_name (c) << " base" << endl
           << "//" << endl;

        os << "if (" << (obj ? "object" : "composite_value") << "_traits< " <<
          class_fq_name (c) << " >::grow (i, t + " << index_ << "UL))" << endl
           << "grew = true;"
           << endl;

        index_ += column_count (c).total;
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

        os << "// " << class_name (c) << " base" << endl
           << "//" << endl;

        // If the derived class is readonly, then we will never be
        // called with sk == statement_update.
        //
        bool check (readonly (c) && !readonly (*context::top_object));

        if (check)
          os << "if (sk != statement_update)"
             << "{";

        os << "if (" << (obj ? "object" : "composite_value") << "_traits< " <<
          class_fq_name (c) << " >::init (i, o, sk))" << endl
           << "grew = true;";

        if (check)
          os << "}";
        else
          os << endl;
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

        os << "// " << class_name (c) << " base" << endl
           << "//" << endl
           << (obj ? "object" : "composite_value") << "_traits< " <<
          class_fq_name (c) << " >::init (o, i, db);"
           << endl;
      }
    };

    // Member-specific traits types for container members.
    //
    struct container_traits: object_members_base, virtual context
    {
      typedef container_traits base;

      container_traits (semantics::class_& c)
          : object_members_base (true, true, false), c_ (c)
      {
        string const& type (class_fq_name (c));

        if (object (c))
          scope_ = "access::object_traits< " + type + " >";
        else
          scope_ = "access::composite_value_traits< " + type + " >";
      }

      // Unless the database system can execute several interleaving
      // statements, cache the result set.
      //
      virtual void
      cache_result (string const& statement)
      {
        os << statement << ".cache ();";
      }

      // Additional code that need to be executed following the call to
      // init_value.
      //
      virtual void
      init_value_extra ()
      {
      }

      virtual void
      process_statement_columns (statement_columns&, statement_kind)
      {
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

              if (generate_grow)
                grow = grow || context::grow (m, *it, "index");
            }

            break;
          }
        case ck_map:
        case ck_multimap:
          {
            kt = &container_kt (t);

            if (generate_grow)
              grow = grow || context::grow (m, *kt, "key");

            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        if (generate_grow)
          grow = grow || context::grow (m, vt, "value");

        bool eager_ptr (is_a (member_path_,
                              member_scope_,
                              test_eager_pointer,
                              vt,
                              "value"));
        if (!eager_ptr)
        {
          if (semantics::class_* cvt = composite_wrapper (vt))
            eager_ptr = has_a (*cvt, test_eager_pointer);
        }

        string name (flat_prefix_ + public_name (m) + "_traits");
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
          os << "const char " << scope <<
            "::select_all_statement[] =" << endl;

          if (inverse)
          {
            semantics::class_* c (object_pointer (vt));

            string inv_table; // Other table name.
            string inv_id;    // Other id column.
            string inv_fid;   // Other foreign id column (ref to us).
            statement_columns sc;

            if (container (*im))
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

              sc.push_back (statement_column (
                              inv_table + "." + inv_id, *im, "id"));
            }
            else
            {
              // many(i)-to-one
              //
              semantics::data_member& id (*id_member (*c));

              inv_table = table_qname (*c);
              inv_id = column_qname (id);
              inv_fid = column_qname (*im);

              sc.push_back (statement_column (inv_table + "." + inv_id, id));
            }

            process_statement_columns (sc, statement_select);

            os << strlit ("SELECT ") << endl;

            for (statement_columns::const_iterator i (sc.begin ()),
                   e (sc.end ()); i != e;)
            {
              string const& c (i->column);
              os << strlit (c + (++i != e ? "," : "")) << endl;
            }

            instance<query_parameters> qp;
            os << strlit (" FROM " + inv_table +
                          " WHERE " + inv_table + "." + inv_fid + "=" +
                          qp->next ());
          }
          else
          {
            statement_columns sc;
            statement_kind sk (statement_select); // Imperfect forwarding.
            instance<object_columns> t (table, sk, sc);

            switch (ck)
            {
            case ck_ordered:
              {
                if (ordered)
                {
                  string const& col (column_qname (m, "index", "index"));
                  t->column (m, "index", table, col);
                }
                break;
              }
            case ck_map:
            case ck_multimap:
              {
                if (semantics::class_* ckt = composite_wrapper (*kt))
                  t->traverse (m, *ckt, "key", "key");
                else
                {
                  string const& col (column_qname (m, "key", "key"));
                  t->column (m, "key", table, col);
                }
                break;
              }
            case ck_set:
            case ck_multiset:
              {
                break;
              }
            }

            if (semantics::class_* cvt = composite_wrapper (vt))
              t->traverse (m, *cvt, "value", "value");
            else
            {
              string const& col (column_qname (m, "value", "value"));
              t->column (m, "value", table, col);
            }

            process_statement_columns (sc, statement_select);

            os << strlit ("SELECT ") << endl;

            for (statement_columns::const_iterator i (sc.begin ()),
                   e (sc.end ()); i != e;)
            {
              string const& c (i->column);
              os << strlit (c + (++i != e ? "," : "")) << endl;
            }

            instance<query_parameters> qp;
            string const& id_col (column_qname (m, "id", "object_id"));

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
          os << "const char " << scope <<
            "::insert_one_statement[] =" << endl;

          if (inverse)
            os << strlit ("") << ";"
               << endl;
          else
          {
            statement_columns sc;
            sc.push_back (
              statement_column (
                column_qname (m, "id", "object_id"), m, "id"));

            statement_kind sk (statement_insert); // Imperfect forwarding.
            instance<object_columns> t (sk, sc);

            switch (ck)
            {
            case ck_ordered:
              {
                if (ordered)
                  t->column (
                    m, "index", "", column_qname (m, "index", "index"));
                break;
              }
            case ck_map:
            case ck_multimap:
              {
                if (semantics::class_* ckt = composite_wrapper (*kt))
                  t->traverse (m, *ckt, "key", "key");
                else
                  t->column (m, "key", "", column_qname (m, "key", "key"));
                break;
              }
            case ck_set:
            case ck_multiset:
              {
                break;
              }
            }

            if (semantics::class_* cvt = composite_wrapper (vt))
              t->traverse (m, *cvt, "value", "value");
            else
              t->column (m, "value", "", column_qname (m, "value", "value"));

            process_statement_columns (sc, statement_insert);

            os << strlit ("INSERT INTO " + table + " (") << endl;

            for (statement_columns::const_iterator i (sc.begin ()),
                   e (sc.end ()); i != e;)
            {
              string const& c (i->column);
              os << strlit (c + (++i != e ? "," : ")")) << endl;
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

            os << strlit (" VALUES (" + values + ")") << ";"
               << endl;
          }

          // delete_all_statement
          //
          os << "const char " << scope <<
            "::delete_all_statement[] =" << endl;

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
          // Would need statement_kind if this is enabled.
          //
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
             << "using namespace " << db << ";"
             << endl
            // In the case of containers, insert and select column sets are
            // the same since we can't have inverse members as container
            // elements.
            //
             << "statement_kind sk (statement_select);"
             << "ODB_POTENTIALLY_UNUSED (sk);"
             << endl
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
                os << "n += " << column_count (*c).total << "UL;"
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
        if (generate_grow)
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
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "init (data_image_type& i, const key_type& k, " <<
                "const value_type& v)";
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "init (data_image_type& i, const value_type& v)";
              break;
            }
          }

          os << "{"
             << "using namespace " << db << ";"
             << endl
             << "statement_kind sk (statement_insert);"
             << "ODB_POTENTIALLY_UNUSED (sk);"
             << endl
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

                instance<init_image_member> im (
                  "index_", "j", *it, "index_type", "index");
                im->traverse (m);
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "// key" << endl
                 << "//" << endl;

              instance<init_image_member> im (
                "key_", "k", *kt, "key_type", "key");
              im->traverse (m);

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
               << "sts.select_image_binding ().version++;"
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


        init_value_extra ();

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
             << "sts.select_image_binding ().version++;"
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
             << "sts.select_image_binding ().version++;"
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
             << "sts.select_image_binding ().version++;"
             << "}"
             << "sts.id_binding (id);"
             << "functions_type& fs (sts.functions ());";

          if (ck == ck_ordered)
            os << "fs.ordered (" << (ordered ? "true" : "false") << ");";

          os << "container_traits_type::persist (c, fs);"
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
           << "sts.select_image_binding ().version++;"
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
             << "sts.select_image_binding ().version++;"
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

        os << "container_traits_type::load (c, more, fs);"
           << "}";

        // update
        //
        if (!(inverse || readonly (member_path_, member_scope_)))
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
             << "sts.select_image_binding ().version++;"
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

          os << "container_traits_type::update (c, fs);"
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

          os << "container_traits_type::erase (fs);"
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
          : object_members_base (true, false, false)
      {
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type&)
      {
        string traits (flat_prefix_ + public_name (m) + "_traits");
        os << db << "::container_statements_impl< " << traits << " > " <<
          flat_prefix_ << m.name () << ";";
      }
    };

    struct container_cache_init_members: object_members_base, virtual context
    {
      typedef container_cache_init_members base;

      container_cache_init_members ()
          : object_members_base (true, false, false), first_ (true)
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

        os << flat_prefix_ << m.name () << " (c)";
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
          : object_members_base (true, false, false),
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
          semantics::names* hint;
          semantics::type& t (utype (*m, hint));

          // Because we cannot have nested containers, m.type () should
          // be the same as w.
          //
          assert (&t == w);

          string const& type (t.fq_name (hint));

          if (call_ == load_call && const_type (m->type ()))
            obj_prefix_ = "const_cast< " + type + "& > (\n" +
              obj_prefix_ + ")";

          obj_prefix_ = "wrapper_traits< " + type + " >::" +
            (call_ == load_call ? "set_ref" : "get_ref") +
            " (\n" + obj_prefix_ + ")";
        }
        else if (call_ == load_call && const_type (m->type ()))
        {
          obj_prefix_ = "const_cast< " + class_fq_name (c) + "& > (\n" +
            obj_prefix_ + ")";
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
        string sts_name (flat_prefix_ + name);
        string traits (flat_prefix_ + public_name (m) + "_traits");

        if (call_ == load_call && const_type (m.type ()))
        {

        }

        semantics::names* hint;
        semantics::type& t (utype (m, hint));

        // If this is a wrapped container, then we need to "unwrap" it.
        //
        if (wrapper (t))
        {
          string const& type (t.fq_name (hint));

          // We cannot use traits::container_type here.
          //
          if (call_ == load_call && const_type (m.type ()))
            obj_name = "const_cast< " + type + "& > (\n" + obj_name + ")";

          obj_name = "wrapper_traits< " + type + " >::" +
            (call_ == load_call ? "set_ref" : "get_ref") +
            " (\n" + obj_name + ")";
        }
        else if (call_ == load_call && const_type (m.type ()))
        {
          obj_name = "const_cast< " + traits + "::container_type& > (\n" +
            obj_name + ")";
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
            if (!(inverse || readonly (member_path_, member_scope_)))
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
      persist_statement_params (string& params, query_parameters& qp)
          : params_ (params), count_ (0), qp_ (qp)
      {
      }

      virtual void
      traverse_simple (semantics::data_member& m)
      {
        if (!inverse (m))
        {
          string p;

          if (version (m))
            p = "1";
          else if (id (m) && auto_ (m))
            p = qp_.auto_id ();
          else
            p = qp_.next ();

          if (!p.empty ())
          {
            if (count_++ != 0)
              params_ += ',';

            params_ += p;
          }
        }
      }

    private:
      string& params_;
      size_t count_;
      query_parameters& qp_;
    };

    //@@ (im)-perfect forwarding.
    //
    static schema_format format_embedded (schema_format::embedded);

    //
    //
    struct class_: traversal::class_, virtual context
    {
      typedef class_ base;

      class_ ()
          : grow_base_ (index_),
            grow_member_ (index_),
            bind_id_member_ ("id_"),
            bind_version_member_ ("version_"),
            init_id_image_member_ ("id_", "id"),
            init_version_image_member_ ("version_", "(*v)"),
            init_id_value_member_ ("id"),
            init_version_value_member_ ("v"),
            stream_ (emitter_),
            drop_model_ (emitter_, stream_, format_embedded),
            drop_table_ (emitter_, stream_, format_embedded),
            drop_index_ (emitter_, stream_, format_embedded),
            create_model_ (emitter_, stream_, format_embedded),
            create_table_ (emitter_, stream_, format_embedded),
            create_index_ (emitter_, stream_, format_embedded)
      {
        init ();
      }

      class_ (class_ const&)
          : root_context (), //@@ -Wextra
            context (),
            grow_base_ (index_),
            grow_member_ (index_),
            bind_id_member_ ("id_"),
            bind_version_member_ ("version_"),
            init_id_image_member_ ("id_", "id"),
            init_version_image_member_ ("version_", "(*v)"),
            init_id_value_member_ ("id"),
            init_version_value_member_ ("v"),
            stream_ (emitter_),
            drop_model_ (emitter_, stream_, format_embedded),
            drop_table_ (emitter_, stream_, format_embedded),
            drop_index_ (emitter_, stream_, format_embedded),
            create_model_ (emitter_, stream_, format_embedded),
            create_table_ (emitter_, stream_, format_embedded),
            create_index_ (emitter_, stream_, format_embedded)
      {
        init ();
      }

      void
      init ()
      {
        if (generate_grow)
        {
          grow_base_inherits_ >> grow_base_;
          grow_member_names_ >> grow_member_;
        }

        bind_base_inherits_ >> bind_base_;
        bind_member_names_ >> bind_member_;

        init_image_base_inherits_ >> init_image_base_;
        init_image_member_names_ >> init_image_member_;

        init_value_base_inherits_ >> init_value_base_;
        init_value_member_names_ >> init_value_member_;

        if (embedded_schema)
        {
          drop_model_ >> drop_names_;
          drop_names_ >> drop_table_;
          drop_names_ >> drop_index_;

          create_model_ >> create_names_;
          create_names_ >> create_table_;
          create_names_ >> create_index_;
        }
      }

      virtual void
      init_auto_id (semantics::data_member&, // id member
                    string const&)           // image variable prefix
      {
        if (insert_send_auto_id)
          assert (false);
      }

      virtual void
      init_image_pre (type&)
      {
      }

      virtual void
      init_value_extra ()
      {
      }

      virtual void
      traverse (type& c)
      {
        if (class_file (c) != unit.file ())
          return;

        context::top_object = context::cur_object = &c;

        if (object (c))
          traverse_object (c);
        else if (view (c))
          traverse_view (c);
        else if (composite (c))
          traverse_composite (c);

        context::top_object = context::cur_object = 0;
      }

      //
      // statements
      //

      enum persist_position
      {
        persist_after_columns,
        persist_after_values
      };

      virtual void
      persist_statement_extra (type&, query_parameters&, persist_position)
      {
      }

      //
      // common
      //

      virtual void
      post_query_ (type&)
      {
      }

      virtual void
      process_statement_columns (statement_columns&, statement_kind)
      {
      }

      //
      // object
      //

      virtual void
      object_extra (type&)
      {
      }

      // By default we free statement result immediately after fetch.
      //
      virtual void
      free_statement_result_immediate ()
      {
        os << "st.free_result ();";
      }

      virtual void
      free_statement_result_delayed ()
      {
      }

      virtual void
      object_query_statement_ctor_args (type&)
      {
        os << "sts.connection ()," << endl
           << "query_statement + q.clause ()," << endl
           << "q.parameters_binding ()," << endl
           << "imb";
      }

      virtual void
      object_erase_query_statement_ctor_args (type&)
      {
        os << "conn," << endl
           << "erase_query_statement + q.clause ()," << endl
           << "q.parameters_binding ()";
      }

      virtual void
      traverse_object (type& c)
      {
        bool abstract (context::abstract (c));
        string const& type (class_fq_name (c));
        string traits ("access::object_traits< " + type + " >");

        bool has_ptr (has_a (c, test_pointer));

        semantics::data_member* id (id_member (c));
        bool auto_id (id ? id->count ("auto") : false);
        bool base_id (id ? &id->scope () != &c : false); // Comes from base.

        semantics::data_member* optimistic (context::optimistic (c));

        bool grow (false);
        bool grow_id (false);

        if (generate_grow)
        {
          grow = context::grow (c);
          grow_id = (id ? context::grow (*id) : false) ||
            (optimistic ? context::grow (*optimistic) : false);
        }

        column_count_type const& cc (column_count (c));

        os << "// " << class_name (c) << endl
           << "//" << endl
           << endl;

        object_extra (c);

        //
        // Query.
        //

        if (options.generate_query ())
        {
          // query_columns_base
          //
          if (has_ptr)
          {
            instance<query_columns_base> t (c);
            t->traverse (c);
          }
        }

        //
        // Containers (abstract and concrete).
        //
        bool containers (has_a (c, test_container));
        bool straight_containers (false);
        bool straight_readwrite_containers (false);

        if (containers)
        {
          containers = true;
          size_t scn (has_a (c, test_straight_container));

          if (scn != 0)
          {
            straight_containers = true;

            // Inverse containers cannot be marked readonly.
            //
            straight_readwrite_containers =
              scn > has_a (c, test_readonly_container);
          }
        }

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

        if (id != 0 && optimistic != 0 && !base_id)
        {
          os << traits << "::version_type" << endl
             << traits << "::" << endl
             << "version (const image_type& i)"
             << "{"
             << "version_type v;";
          init_version_value_member_->traverse (*optimistic);
          os << "return v;"
             << "}";
        }

        // grow ()
        //
        if (generate_grow)
        {
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
        }

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (" << bind_vector << " b, image_type& i, " <<
          db << "::statement_kind sk)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (sk);"
           << endl
           << "using namespace " << db << ";"
           << endl;

        if (readonly (c))
          os << "assert (sk != statement_update);"
             << endl;

        os << "std::size_t n (0);"
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

          if (optimistic != 0)
          {
            os << "n++;" //@@ composite id
               << endl;

            bind_version_member_->traverse (*optimistic);
          }

          os << "}";
        }

        // init (image, object)
        //
        os << "bool " << traits << "::" << endl
           << "init (image_type& i, const object_type& o, " <<
          db << "::statement_kind sk)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << "ODB_POTENTIALLY_UNUSED (sk);"
           << endl
           << "using namespace " << db << ";"
           << endl;

        if (readonly (c))
          os << "assert (sk != statement_update);"
             << endl;

        init_image_pre (c);

        os << "bool grew (false);"
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
             << "init (id_image_type& i, const id_type& id" <<
            (optimistic != 0 ? ", const version_type* v" : "") << ")"
             << "{";

          if (grow_id)
            os << "bool grew (false);"
               << endl;

          init_id_image_member_->traverse (*id);

          if (optimistic != 0)
          {
            // Here we rely on the fact that init_image_member
            // always wraps the statements in a block.
            //
            os << "if (v != 0)";
            init_version_image_member_->traverse (*optimistic);
          }

          if (grow_id)
            os << "if (grew)" << endl
               << "i.version++;";

          os << "}";
        }

        //
        // The rest only applies to concrete objects.
        //
        if (abstract)
          return;

        //
        // Containers (concrete).
        //

        // Statement cache (definition).
        //
        if (id != 0)
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

        // persist_statement
        //
        {
          statement_columns sc;
          {
            statement_kind sk (statement_insert); // Imperfect forwarding.
            instance<object_columns> ct (sk, sc);
            ct->traverse (c);
            process_statement_columns (sc, statement_insert);
          }

          bool dv (sc.empty ()); // The DEFAULT VALUES syntax.

          os << "const char " << traits << "::persist_statement[] " <<
            "=" << endl
             << strlit ("INSERT INTO " + table_qname(c) +
                        (dv ? "" : " (")) << endl;

          for (statement_columns::const_iterator i (sc.begin ()),
                 e (sc.end ()); i != e;)
          {
            string const& c (i->column);
            os << strlit (c + (++i != e ? "," : ")")) << endl;
          }

          instance<query_parameters> qp;

          persist_statement_extra (c, *qp, persist_after_columns);

          if (!dv)
          {
            string values;
            instance<persist_statement_params> pt (values, *qp);
            pt->traverse (c);
            os << strlit (" VALUES (" + values + ")");
          }
          else
            os << strlit (" DEFAULT VALUES");

          persist_statement_extra (c, *qp, persist_after_values);

          os << ";"
             << endl;
        }

        if (id != 0)
        {
          string const& id_col (column_qname (*id));

          // find_statement
          //
          {
            statement_columns sc;
            {
              statement_kind sk (statement_select); // Imperfect forwarding.
              instance<object_columns> t (table, sk, sc);
              t->traverse (c);
              process_statement_columns (sc, statement_select);
            }

            os << "const char " << traits << "::find_statement[] =" << endl
               << strlit ("SELECT ") << endl;

            for (statement_columns::const_iterator i (sc.begin ()),
                   e (sc.end ()); i != e;)
            {
              string const& c (i->column);
              os << strlit (c + (++i != e ? "," : "")) << endl;
            }

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
          if (cc.total != cc.id + cc.inverse + cc.readonly)
          {
            instance<query_parameters> qp;

            statement_columns sc;
            {
              query_parameters* p (qp.get ()); // Imperfect forwarding.
              statement_kind sk (statement_update); // Imperfect forwarding.
              instance<object_columns> t (sk, sc, p);
              t->traverse (c);
              process_statement_columns (sc, statement_update);
            }

            os << "const char " << traits << "::update_statement[] " <<
              "=" << endl
               << strlit ("UPDATE " + table + " SET ") << endl;

            for (statement_columns::const_iterator i (sc.begin ()),
                   e (sc.end ()); i != e;)
            {
              string const& c (i->column);
              os << strlit (c + (++i != e ? "," : "")) << endl;
            }

            string where (" WHERE " + id_col + "=" + qp->next ());

            if (optimistic != 0)
              where += " AND " + column_qname (*optimistic) + "=" +
                qp->next ();

            os << strlit (where) << ";"
               << endl;
          }

          // erase_statement
          //
          {
            instance<query_parameters> qp;
            os << "const char " << traits << "::erase_statement[] =" << endl
               << strlit ("DELETE FROM " + table) << endl
               << strlit (" WHERE " + id_col + "=" + qp->next ()) << ";"
               << endl;
          }

          if (optimistic != 0)
          {
            instance<query_parameters> qp;

            string where (" WHERE " + id_col + "=" + qp->next ());
            where += " AND " + column_qname (*optimistic) + "=" + qp->next ();

            os << "const char " << traits <<
              "::optimistic_erase_statement[] =" << endl
               << strlit ("DELETE FROM " + table) << endl
               << strlit (where) << ";"
               << endl;
          }
        }

        if (options.generate_query ())
        {
          // query_statement
          //
          statement_columns sc;
          {
            statement_kind sk (statement_select); // Imperfect forwarding.
            instance<object_columns> oc (table, sk, sc);
            oc->traverse (c);
            process_statement_columns (sc, statement_select);
          }

          bool t (true);
          instance<object_joins> oj (c, t); //@@ (im)perfect forwarding
          oj->traverse (c);

          os << "const char " << traits << "::query_statement[] =" << endl
             << strlit ("SELECT ") << endl;

          for (statement_columns::const_iterator i (sc.begin ()),
                 e (sc.end ()); i != e;)
          {
            string const& c (i->column);
            os << strlit (c + (++i != e ? "," : "")) << endl;
          }

          os << strlit (" FROM " + table) << endl;
          oj->write ();
          os << strlit (" ") << ";"
             << endl;

          // erase_query_statement
          //
          os << "const char " << traits << "::erase_query_statement[] =" << endl
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
          os << "const char " << traits << "::table_name[] =" << endl
             << strlit (table_qname (c)) << ";" // Use quoted name.
             << endl;
        }

        // persist ()
        //
        char const* object_statements_type (
          id != 0
          ? "object_statements< object_type >"
          : "object_statements_no_id< object_type >");

        os << "void " << traits << "::" << endl
           << "persist (database&, " << (auto_id ? "" : "const ") <<
          "object_type& obj)"
           << "{"
           << "using namespace " << db << ";"
           << endl
           << db << "::connection& conn (" << endl
           << db << "::transaction::current ().connection ());"
           << object_statements_type << "& sts (" << endl
           << "conn.statement_cache ().find_object<object_type> ());"
           << "image_type& im (sts.image ());"
           << "binding& imb (sts.insert_image_binding ());"
           << endl
           << "if (init (im, obj, statement_insert))" << endl
           << "im.version++;"
           << endl;

        if (auto_id && insert_send_auto_id)
        {
          string const& n (id->name ());
          string var ("im." + n + (n[n.size () - 1] == '_' ? "" : "_"));
          init_auto_id (*id, var);
          os << endl;
        }

        os << "if (im.version != sts.insert_image_version () || " <<
          "imb.version == 0)"
           << "{"
           << "bind (imb.bind, im, statement_insert);"
           << "sts.insert_image_version (im.version);"
           << "imb.version++;"
           << "}"
           << "insert_statement& st (sts.persist_statement ());"
           << "if (!st.execute ())" << endl
           << "throw object_already_persistent ();"
           << endl;

        if (auto_id)
        {
          if (const_type (id->type ()))
            os << "const_cast< id_type& > (obj." << id->name () << ")";
          else
            os << "obj." << id->name ();

          os << " = static_cast< id_type > (st.id ());"
             << endl;
        }

        if (optimistic != 0)
        {
          // Set the version in the object member.
          //
          if (!auto_id || const_type (optimistic->type ()))
            os << "const_cast< version_type& > (" <<
              "obj." << optimistic->name () << ") = 1;";
          else
            os << "obj." << optimistic->name () << " = 1;";

          os << endl;
        }

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
        if (id != 0 && !readonly (c))
        {
          os << "void " << traits << "::" << endl
             << "update (database&, const object_type& obj)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << object_statements_type << "& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << endl;

          if (cc.total != cc.id + cc.inverse + cc.readonly)
          {
            // Initialize object and id images.
            //
            os << "id_image_type& i (sts.id_image ());";

            if (optimistic == 0)
              os << "init (i, obj." << id->name () << ");";
            else
              os << "init (i, obj." << id->name () << ", &obj." <<
                optimistic->name () << ");";

            os << endl
               << "image_type& im (sts.image ());"
               << "if (init (im, obj, statement_update))" << endl
               << "im.version++;"
               << endl;

            // Update binding is bound to two images (object and id)
            // so we have to track both versions.
            //
            os << "bool u (false);" // Avoid incrementing version twice.
               << "binding& imb (sts.update_image_binding ());"
               << "if (im.version != sts.update_image_version () || " <<
              "imb.version == 0)"
               << "{"
               << "bind (imb.bind, im, statement_update);"
               << "sts.update_image_version (im.version);"
               << "imb.version++;"
               << "u = true;"
               << "}";

            // To update the id part of the update binding we have to do
            // it indirectly via the id binding, which just points to the
            // suffix of the update bind array (see object_statements).
            //
            os << "binding& idb (sts.id_image_binding ());"
               << "if (i.version != sts.update_id_image_version () || " <<
              "idb.version == 0)"
               << "{"
              // If the id binding is up-to-date, then that means update
              // binding is too and we just need to update the versions.
              //
               << "if (i.version != sts.id_image_version () || " <<
              "idb.version == 0)"
               << "{"
               << "bind (idb.bind, i);"
              // Update the id binding versions since we may use them later
              // to update containers.
              //
               << "sts.id_image_version (i.version);"
               << "idb.version++;"
               << "}"
               << "sts.update_id_image_version (i.version);"
               << endl
               << "if (!u)" << endl
               << "imb.version++;"
               << "}";

            os << "if (sts.update_statement ().execute () == 0)" << endl;

            if (optimistic == 0)
              os << "throw object_not_persistent ();";
            else
              os << "throw object_changed ();";

            os << endl;
          }
          else
          {
            // We don't have any columns to update. Note that we still have
            // to make sure this object exists in the database. For that we
            // will run the SELECT query using the find_() function.
            //
            os << "if (!find_ (sts, obj." << id->name () << "))" << endl
               << "throw object_not_persistent ();"
               << endl;

            free_statement_result_delayed ();

            if (straight_readwrite_containers)
              os << "binding& idb (sts.id_image_binding ());"
                 << endl;
          }

          if (straight_readwrite_containers)
          {
            instance<container_calls> t (container_calls::update_call);
            t->traverse (c);
          }

          if (optimistic != 0)
          {
            // Update version in the object member.
            //
            os << "const_cast< version_type& > (" <<
              "obj." << optimistic->name () << ")++;";
          }

          os << "}";
        }

        // erase (id_type)
        //
        if (id != 0)
        {
          os << "void " << traits << "::" << endl
             << "erase (database&, const id_type& id)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << object_statements_type << "& sts (" << endl
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
          }

          os << "if (sts.erase_statement ().execute () != 1)" << endl
             << "throw object_not_persistent ();";

          os << "}";
        }

        // erase (object_type)
        //
        if (id != 0 && optimistic != 0)
        {
          os << "void " << traits << "::" << endl
             << "erase (database&, const object_type& obj)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << object_statements_type << "& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << endl;

          // Initialize id + managed column image.
          //
          os << "id_image_type& i (sts.id_image ());"
             << "init (i, obj." << id->name () << ", &obj." <<
            optimistic->name () << ");"
             << endl;

          // To update the id part of the optimistic id binding we have
          // to do it indirectly via the id binding, since both id and
          // optimistic id bindings just point to the suffix of the
          // update bind array (see object_statements).
          //
          os << "binding& idb (sts.id_image_binding ());"
             << "binding& oidb (sts.optimistic_id_image_binding ());"
             << "if (i.version != sts.optimistic_id_image_version () || " <<
            "oidb.version == 0)"
             << "{"
            // If the id binding is up-to-date, then that means optimistic
            // id binding is too and we just need to update the versions.
            //
             << "if (i.version != sts.id_image_version () || idb.version == 0)"
             << "{"
             << "bind (idb.bind, i);"
            // Update the id binding versions since we may use them later
            // to delete containers.
            //
             << "sts.id_image_version (i.version);"
             << "idb.version++;"
             << "}"
             << "sts.optimistic_id_image_version (i.version);"
             << "oidb.version++;"
             << "}";

          // Erase containers first so that there are no reference
          // violations (we don't want to rely on ON DELETE CASCADE
          // here since in case of a custom schema, it might not be
          // there).
          //
          if (straight_containers)
          {
            // Things get complicated here: we don't want to trash the
            // containers and then find out that the versions don't match
            // and we therefore cannot delete the object. After all, there
            // is no guarantee that the user will abort the transaction.
            // In fact, a perfectly reasonable scenario is to reload the
            // object, re-apply the changes, and commit the transaction.
            //
            // There doesn't seem to be anything better than first making
            // sure we can delete the object, then deleting the container
            // data, and then deleting the object. To check that we can
            // delete the object we are going to use find_() and then
            // compare the versions. A special-purpose SELECT query would
            // have been more efficient but it would complicated and bloat
            // things significantly.
            //

            os << "if (!find_ (sts, obj." << id->name () << "))" << endl
               << "throw object_changed ();"
               << endl;

            free_statement_result_delayed ();

            os << "if (version (sts.image ()) != obj." <<
              optimistic->name () << ")" << endl
               << "throw object_changed ();"
               << endl;

            instance<container_calls> t (container_calls::erase_call);
            t->traverse (c);
          }

          os << "if (sts.optimistic_erase_statement ().execute () != 1)" << endl
             << "throw object_changed ();"
             << "}";
        }

        // find (id)
        //
        if (id != 0 && c.default_ctor ())
        {
          os << traits << "::pointer_type" << endl
             << traits << "::" << endl
             << "find (database& db, const id_type& id)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << object_statements_type << "& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << object_statements_type << "::auto_lock l (sts);"
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
             << "init (obj, sts.image (), db);";

          init_value_extra ();
          free_statement_result_delayed ();

          os << "load_ (sts, obj);"
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

        // find (id, obj)
        //
        if (id != 0)
        {
          os << "bool " << traits << "::" << endl
             << "find (database& db, const id_type& id, object_type& obj)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << object_statements_type << "& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << endl
            // This can only be top-level call so auto_lock must succeed.
            //
             << object_statements_type << "::auto_lock l (sts);"
             << endl;

          os << "if (!find_ (sts, id))" << endl
             << "return false;"
             << endl
             << "reference_cache_traits< object_type >::insert_guard ig (" << endl
             << "reference_cache_traits< object_type >::insert (db, id, obj));"
             << endl
             << "callback (db, obj, callback_event::pre_load);"
             << "init (obj, sts.image (), db);";

          init_value_extra ();
          free_statement_result_delayed ();

          os << "load_ (sts, obj);"
             << "sts.load_delayed ();"
             << "l.unlock ();"
             << "callback (db, obj, callback_event::post_load);"
             << "ig.release ();"
             << "return true;"
             << "}";
        }

        // reload()
        //
        if (id != 0)
        {
          os << "bool " << traits << "::" << endl
             << "reload (database& db, object_type& obj)"
             << "{"
             << "using namespace " << db << ";"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << object_statements_type << "& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << endl
            // This can only be top-level call so auto_lock must succeed.
            //
             << object_statements_type << "::auto_lock l (sts);"
             << endl;

          os << "if (!find_ (sts, obj." << id->name () << "))" << endl
             << "return false;"
             << endl;

          if (optimistic != 0)
          {
            os << "if (version (sts.image ()) == obj." <<
              optimistic->name () << ")"
               << "{";

            free_statement_result_delayed ();

            os << "return true;"
               << "}";
          }

          os << "callback (db, obj, callback_event::pre_load);"
             << "init (obj, sts.image (), db);";

          init_value_extra ();
          free_statement_result_delayed ();

          os << "load_ (sts, obj);"
             << "sts.load_delayed ();"
             << "l.unlock ();"
             << "callback (db, obj, callback_event::post_load);"
             << "return true;"
             << "}";
        }

        // find_ ()
        //
        if (id != 0)
        {
          os << "bool " << traits << "::" << endl
             << "find_ (" << db << "::" << object_statements_type << "& " <<
            "sts, const id_type& id)"
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
             << "binding& imb (sts.select_image_binding ());"
             << endl
             << "if (im.version != sts.select_image_version () || " <<
            "imb.version == 0)"
             << "{"
             << "bind (imb.bind, im, statement_select);"
             << "sts.select_image_version (im.version);"
             << "imb.version++;"
             << "}"
             << "select_statement& st (sts.find_statement ());"
             << "st.execute ();"
             << "select_statement::result r (st.fetch ());";

          if (grow)
            os << endl
               << "if (r == select_statement::truncated)"
               << "{"
               << "if (grow (im, sts.select_image_truncated ()))" << endl
               << "im.version++;"
               << endl
               << "if (im.version != sts.select_image_version ())"
               << "{"
               << "bind (imb.bind, im, statement_select);"
               << "sts.select_image_version (im.version);"
               << "imb.version++;"
               << "st.refetch ();"
               << "}"
               << "}";

          free_statement_result_immediate ();

          os << "return r != select_statement::no_data;"
             << "}";
        }

        // load_()
        //
        if (containers)
        {
          os << "void " << traits << "::" << endl
             << "load_ (" << db << "::" << object_statements_type << "& " <<
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
          os << "result< " << traits << "::object_type >" << endl
             << traits << "::" << endl
             << "query (database&, const query_base_type& q)"
             << "{"
             << "using namespace " << db << ";"
             << "using odb::details::shared;"
             << "using odb::details::shared_ptr;"
             << endl
             << db << "::connection& conn (" << endl
             << db << "::transaction::current ().connection ());"
             << endl
             << object_statements_type << "& sts (" << endl
             << "conn.statement_cache ().find_object<object_type> ());"
             << endl
             << "image_type& im (sts.image ());"
             << "binding& imb (sts.select_image_binding ());"
             << endl
             << "if (im.version != sts.select_image_version () || " <<
            "imb.version == 0)"
             << "{"
             << "bind (imb.bind, im, statement_select);"
             << "sts.select_image_version (im.version);"
             << "imb.version++;"
             << "}"
             << "shared_ptr<select_statement> st (" << endl
             << "new (shared) select_statement (" << endl;

          object_query_statement_ctor_args (c);

          os << "));" << endl
             << "st->execute ();";

          post_query_ (c);

          char const* result_type (
            id != 0
            ? "object_result_impl<object_type>"
            : "object_result_impl_no_id<object_type>");

          os << endl
             << "shared_ptr< odb::" << result_type << " > r (" << endl
             << "new (shared) " << db << "::" << result_type << " (" << endl
             << "q, st, sts));"
             << endl
             << "return result<object_type> (r);"
             << "}";

          // erase_query
          //
          os << "unsigned long long " << traits << "::" << endl
             << "erase_query (database&, const query_base_type& q)"
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
          if (l.empty ())
            return; // Ignore empty lines.

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
            os << strlit (line_ + '\n') << endl;

          line_ = l;
        }

        virtual void
        post ()
        {
          if (!first_) // Ignore empty statements.
            os << strlit (line_) << ");" << endl;
        }

      private:
        std::string line_;
        bool first_;
        bool empty_;
        bool new_pass_;
        unsigned short pass_;
      };

      virtual void
      schema (type& c)
      {
        typedef sema_rel::model::names_iterator iterator;

        iterator begin (c.get<iterator> ("model-range-first"));
        iterator end (c.get<iterator> ("model-range-last"));

        if (begin == model->names_end ())
          return; // This class doesn't have any model entities (e.g.,
                  // a second class mapped to the same table).

        ++end; // Transform the range from [begin, end] to [begin, end).

        string const& type (class_fq_name (c));
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
            emitter_.pass (pass);
            drop_model_->pass (pass);
            drop_table_->pass (pass);
            drop_index_->pass (pass);

            drop_model_->traverse (begin, end);

            close = close || !emitter_.empty ();
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
            emitter_.pass (pass);
            create_model_->pass (pass);
            create_table_->pass (pass);
            create_index_->pass (pass);

            create_model_->traverse (begin, end);

            close = close || !emitter_.empty ();
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
           << strlit (options.schema_name ()) << "," << endl
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
           << "qs.clause ()," << endl
           << "qs.parameters_binding ()," << endl
           << "imb";
      }

      virtual void
      traverse_view (type& c)
      {
        string const& type (class_fq_name (c));
        string traits ("access::view_traits< " + type + " >");

        os << "// " << class_name (c) << endl
           << "//" << endl
           << endl;

        view_extra (c);

        //
        // Query.
        //

        // query_type
        //
        size_t obj_count (c.get<size_t> ("object-count"));

        if (obj_count != 0)
        {
          view_objects& objs (c.get<view_objects> ("objects"));

          if (obj_count > 1)
          {
            for (view_objects::const_iterator i (objs.begin ());
                 i < objs.end ();
                 ++i)
            {
              if (i->kind != view_object::object)
                continue; // Skip tables.

              qname const& t (table_name (*i->obj));

              if (!i->alias.empty () &&
                  (t.qualified () || i->alias != t.uname ()))
                os << "const char " << traits << "::query_columns::" << endl
                   << i->alias << "_alias_[] = " <<
                  strlit (quote_id (i->alias)) << ";"
                   << endl;
            }
          }
          else
          {
            // For a single object view we generate a shortcut without
            // an intermediate typedef.
            //
            view_object const* vo (0);
            for (view_objects::const_iterator i (objs.begin ());
                 vo == 0 && i < objs.end ();
                 ++i)
            {
              if (i->kind == view_object::object)
                vo = &*i;
            }

            qname const& t (table_name (*vo->obj));

            if (!vo->alias.empty () &&
                (t.qualified () || vo->alias != t.uname ()))
              os << "const char " << traits << "::" << endl
                 << "query_alias[] = " << strlit (quote_id (vo->alias)) << ";"
                 << endl;
          }
        }

        //
        // Functions.
        //

        // grow ()
        //
        if (generate_grow)
        {
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
        }

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (" << bind_vector << " b, image_type& i)"
           << "{"
           << "using namespace " << db << ";"
           << endl
           << db << "::statement_kind sk (statement_select);"
           << "ODB_POTENTIALLY_UNUSED (sk);"
           << endl
           << "std::size_t n (0);"
           << endl;

        names (c, bind_member_names_);

        os << "}";

        // init (view, image)
        //
        os << "void " << traits << "::" << endl
           << "init (view_type& o, const image_type& i, database& db)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << endl;

        names (c, init_value_member_names_);

        os << "}";

        // query_statement()
        //
        view_query& vq (c.get<view_query> ("query"));

        if (vq.kind != view_query::runtime)
        {
          os << traits << "::query_base_type" << endl
             << traits << "::" << endl
             << "query_statement (const query_base_type& q)"
             << "{";

          if (vq.kind == view_query::complete)
          {
            os << "query_base_type r (" << endl;

            bool ph (false);

            if (!vq.literal.empty ())
            {
              // See if we have the '(?)' placeholder.
              //
              // @@ Ideally we would need to make sure we don't match
              // this inside strings and quoted identifier. So the
              // proper way to handle this would be to tokenize the
              // statement using sql_lexer, once it is complete enough.
              //
              string::size_type p (vq.literal.find ("(?)"));

              if (p != string::npos)
              {
                ph = true;
                os << strlit (string (vq.literal, 0, p + 1)) << " +" << endl
                   << "(q.empty () ? query_base_type::true_expr : q) +" << endl
                   << strlit (string (vq.literal, p + 2));
              }
              else
                os << strlit (vq.literal);
            }
            else
              // Output the pragma location for easier error tracking.
              //
              os << "// From " <<
                location_file (vq.loc).leaf () << ":" <<
                location_line (vq.loc) << ":" <<
                location_column (vq.loc) << endl
                 << translate_expression (
                   c, vq.expr, vq.scope, vq.loc, "query", &ph).value;

            os << ");";

            // If there was no placeholder, add the query condition
            // at the end.
            //
            if (!ph)
              os << "r += q.clause_prefix ();"
                 << "r += q;";
          }
          else // vq.kind == view_query::condition
          {
            statement_columns sc;
            {
              instance<view_columns> t (sc);
              t->traverse (c);
              process_statement_columns (sc, statement_select);
            }

            os << "query_base_type r (" << endl
               << strlit ("SELECT ") << endl;

            for (statement_columns::const_iterator i (sc.begin ()),
                   e (sc.end ()); i != e;)
            {
              string const& c (i->column);
              os << strlit (c + (++i != e ? "," : "")) << endl;
            }

            os << ");"
               << endl;

            // Generate from-list.
            //
            view_objects const& objs (c.get<view_objects> ("objects"));

            for (view_objects::const_iterator i (objs.begin ());
                 i != objs.end ();
                 ++i)
            {
              bool first (i == objs.begin ());
              string l;

              //
              // Tables.
              //

              if (i->kind == view_object::table)
              {
                if (first)
                {
                  l = "FROM ";
                  l += quote_id (i->tbl_name);

                  if (!i->alias.empty ())
                    l += (need_alias_as ? " AS " : " ") + quote_id (i->alias);

                  os << "r += " << strlit (l) << ";"
                     << endl;

                  continue;
                }

                l = "LEFT JOIN ";
                l += quote_id (i->tbl_name);

                if (!i->alias.empty ())
                  l += (need_alias_as ? " AS " : " ") + quote_id (i->alias);

                expression e (
                  translate_expression (
                    c, i->cond, i->scope, i->loc, "table"));

                if (e.kind != expression::literal)
                {
                  error (i->loc)
                    << "invalid join condition in db pragma table" << endl;

                  throw operation_failed ();
                }

                l += " ON";

                os << "r += " << strlit (l) << ";"
                  // Output the pragma location for easier error tracking.
                  //
                   << "// From " <<
                  location_file (i->loc).leaf () << ":" <<
                  location_line (i->loc) << ":" <<
                  location_column (i->loc) << endl
                   << "r += " << e.value << ";"
                   << endl;

                continue;
              }

              //
              // Objects.
              //

              // First object.
              //
              if (first)
              {
                l = "FROM ";
                l += table_qname (*i->obj);

                if (!i->alias.empty ())
                  l += (need_alias_as ? " AS " : " ") + quote_id (i->alias);

                os << "r += " << strlit (l) << ";"
                   << endl;

                continue;
              }

              expression e (
                translate_expression (
                  c, i->cond, i->scope, i->loc, "object"));

              // Literal expression.
              //
              if (e.kind == expression::literal)
              {
                l = "LEFT JOIN ";
                l += table_qname (*i->obj);

                if (!i->alias.empty ())
                  l += (need_alias_as ? " AS " : " ") + quote_id (i->alias);

                l += " ON";

                os << "r += " << strlit (l) << ";"
                  // Output the pragma location for easier error tracking.
                  //
                   << "// From " <<
                  location_file (i->loc).leaf () << ":" <<
                  location_line (i->loc) << ":" <<
                  location_column (i->loc) << endl
                   << "r += " << e.value << ";"
                   << endl;

                continue;
              }

              // We have an object relationship (pointer) for which we need
              // to come up with the corresponding JOIN condition. If this
              // is a to-many relationship, then we first need to JOIN the
              // container table. This code is similar to object_joins.
              //
              using semantics::data_member;

              data_member& m (*e.member_path.back ());

              // Resolve the pointed-to object to view_object and do
              // some sanity checks while at it.
              //
              semantics::class_* c (0);

              if (semantics::type* cont = container (m))
                c = object_pointer (container_vt (*cont));
              else
                c = object_pointer (utype (m));

              view_object const* vo (0);

              // Check if the pointed-to object has been previously
              // associated with this view and is unambiguous. A
              // pointer to ourselves is always assumed to point
              // to this association.
              //
              if (i->obj == c)
                vo = &*i;
              else
              {
                bool ambig (false);

                for (view_objects::const_iterator j (objs.begin ());
                     j != i;
                     ++j)
                {
                  if (j->obj != c)
                    continue;

                  if (vo == 0)
                  {
                    vo = &*j;
                    continue;
                  }

                  // If it is the first ambiguous object, issue the
                  // error.
                  //
                  if (!ambig)
                  {
                    error (i->loc)
                      << "pointed-to object '" << class_name (*c) <<  "' is "
                      << "ambiguous" << endl;

                    info (i->loc)
                      << "candidates are:" << endl;

                    info (vo->loc)
                      << "  '" << vo->name () << "'" << endl;

                    ambig = true;
                  }

                  info (j->loc)
                    << "  '" << j->name () << "'" << endl;
                }

                if (ambig)
                {
                  info (i->loc)
                    << "use the full join condition clause in db pragma "
                    << "object to resolve this ambiguity" << endl;

                  throw operation_failed ();
                }

                if (vo == 0)
                {
                  error (i->loc)
                    << "pointed-to object '" << class_name (*c) << "' "
                    << "specified in the join condition has not been "
                    << "previously associated with this view" << endl;

                  throw operation_failed ();
                }
              }

              // Left and right-hand side table names.
              //
              qname lt (e.vo->alias.empty ()
                        ? table_name (*e.vo->obj)
                        : qname (e.vo->alias));

              qname rt (vo->alias.empty ()
                        ? table_name (*vo->obj)
                        : qname (vo->alias));

              // First join the container table if necessary.
              //
              data_member* im (inverse (m));

              semantics::type* cont (container (im != 0 ? *im : m));

              // Container table.
              //
              string ct;
              if (cont != 0)
              {
                if (im != 0)
                {
                  // For now a direct member can only be directly in
                  // the object scope. When this changes, the inverse()
                  // function would have to return a member path instead
                  // of just a single member.
                  //
                  table_prefix tp (table_name (*vo->obj) + "_", 1);
                  ct = table_qname (*im, tp);
                }
                else
                  ct = table_qname (*e.vo->obj, e.member_path);
              }

              if (cont != 0)
              {
                l = "LEFT JOIN ";
                l += ct;
                l += " ON";
                os << "r += " << strlit (l) << ";";

                // If we are the pointed-to object, then we have to turn
                // things around. This is necessary to have the proper
                // JOIN order. There seems to be a pattern there but
                // it is not yet intuitively clear what it means.
                //
                if (im != 0)
                {
                  if (i->obj == c)
                  {
                    // container.value = pointer.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (*im, "value", "value");
                    l += "=";
                    l += quote_id (lt);
                    l += '.';
                    l += column_qname (*id_member (*e.vo->obj));
                  }
                  else
                  {
                    // container.id = pointed-to.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (*im, "id", "object_id");
                    l += "=";
                    l += quote_id (rt);
                    l += '.';
                    l += column_qname (*id_member (*vo->obj));
                  }
                }
                else
                {
                  if (i->obj == c)
                  {
                    // container.id = pointer.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (m, "id", "object_id");
                    l += "=";
                    l += quote_id (lt);
                    l += '.';
                    l += column_qname (*id_member (*e.vo->obj));
                  }
                  else
                  {
                    // container.value = pointed-to.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (m, "value", "value");
                    l += "=";
                    l += quote_id (rt);
                    l += '.';
                    l += column_qname (*id_member (*vo->obj));
                  }
                }

                os << "r += " << strlit (l) << ";";
              }

              l = "LEFT JOIN ";
              l += table_qname (*i->obj);

              if (!i->alias.empty ())
                l += (need_alias_as ? " AS " : " ") + quote_id (i->alias);

              l += " ON";
              os << "r += " << strlit (l) << ";";

              if (cont != 0)
              {
                if (im != 0)
                {
                  if (i->obj == c)
                  {
                    // container.id = pointed-to.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (*im, "id", "object_id");
                    l += "=";
                    l += quote_id (rt);
                    l += '.';
                    l += column_qname (*id_member (*vo->obj));
                  }
                  else
                  {
                    // container.value = pointer.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (*im, "value", "value");
                    l += "=";
                    l += quote_id (lt);
                    l += '.';
                    l += column_qname (*id_member (*e.vo->obj));
                  }
                }
                else
                {
                  if (i->obj == c)
                  {
                    // container.value = pointed-to.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (m, "value", "value");
                    l += "=";
                    l += quote_id (rt);
                    l += '.';
                    l += column_qname (*id_member (*vo->obj));
                  }
                  else
                  {
                    // container.id = pointer.id
                    //
                    l  = ct;
                    l += '.';
                    l += column_qname (m, "id", "object_id");
                    l += "=";
                    l += quote_id (lt);
                    l += '.';
                    l += column_qname (*id_member (*e.vo->obj));
                  }
                }
              }
              else
              {
                if (im != 0)
                {
                  // our.id = pointed-to.pointer
                  //
                  l  = quote_id (lt);
                  l += '.';
                  l += column_qname (*id_member (*e.vo->obj));
                  l += " = ";
                  l += quote_id (rt);
                  l += '.';
                  l += column_qname (*im);
                }
                else
                {
                  // our.pointer = pointed-to.id
                  //
                  l  = quote_id (lt);
                  l += '.';
                  l += column_qname (e.member_path);
                  l += " = ";
                  l += quote_id (rt);
                  l += '.';
                  l += column_qname (*id_member (*vo->obj));
                }
              }

              os << "r += " << strlit (l) << ";"
                 << endl;
            }

            // Generate the query condition.
            //
            if (!vq.literal.empty () || !vq.expr.empty ())
            {
              os << "query_base_type c (" << endl;

              bool ph (false);

              if (!vq.literal.empty ())
              {
                // See if we have the '(?)' placeholder.
                //
                // @@ Ideally we would need to make sure we don't match
                // this inside strings and quoted identifier. So the
                // proper way to handle this would be to tokenize the
                // statement using sql_lexer, once it is complete enough.
                //
                string::size_type p (vq.literal.find ("(?)"));

                if (p != string::npos)
                {
                  ph = true;
                  os << strlit (string (vq.literal, 0, p + 1))<< " +" << endl
                     << "(q.empty () ? query_base_type::true_expr : q) +" << endl
                     << strlit (string (vq.literal, p + 2));
                }
                else
                  os << strlit (vq.literal);

                os << ");";
              }
              else
              {
                // Output the pragma location for easier error tracking.
                //
                os << "// From " <<
                  location_file (vq.loc).leaf () << ":" <<
                  location_line (vq.loc) << ":" <<
                  location_column (vq.loc) << endl
                   << translate_expression (
                     c, vq.expr, vq.scope, vq.loc, "query", &ph).value;

                os << ");";

                // Optimize the query if it had a placeholder. This gets
                // rid of useless clauses like WHERE TRUE.
                //
                if (ph)
                  os << "c.optimize ();";
              }

              if (!ph)
                os << "c += q;";

              os << "r += c.clause_prefix ();"
                 << "r += c;"
                 << endl;
            }
            else
            {
              os << "r += q.clause_prefix ();"
                 << "r += q;"
                 << endl;
            }
          }

          os << "return r;"
             << "}";
        }

        // query ()
        //
        os << "result< " << traits << "::view_type >" << endl
           << traits << "::" << endl
           << "query (database&, const query_base_type& q)"
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
           << "}";

        if (vq.kind == view_query::runtime)
          os << "const query_base_type& qs (q);";
        else
          os << "const query_base_type& qs (query_statement (q));";

        os << "shared_ptr<select_statement> st (" << endl
           << "new (shared) select_statement (" << endl;

        view_query_statement_ctor_args (c);

        os << "));" << endl
           << "st->execute ();";

        post_query_ (c);

        os << endl
           << "shared_ptr< odb::view_result_impl<view_type> > r (" << endl
           << "new (shared) " << db << "::view_result_impl<view_type> (" << endl
           << "qs, st, sts));"
           << endl
           << "return result<view_type> (r);"
           << "}";
      }

      struct expression
      {
        explicit
        expression (std::string const& v): kind (literal), value (v) {}
        expression (view_object* vo): kind (pointer), vo (vo) {}

        enum kind_type {literal, pointer};

        kind_type kind;
        std::string value;
        data_member_path member_path;
        view_object* vo;
      };

      expression
      translate_expression (type& c,
                            cxx_tokens const&,
                            tree scope,
                            location_t loc,
                            string const& prag,
                            bool* placeholder = 0);
      //
      // composite
      //

      virtual void
      traverse_composite (type& c)
      {
        string const& type (class_fq_name (c));
        string traits ("access::composite_value_traits< " + type + " >");

        os << "// " << class_name (c) << endl
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
        if (generate_grow)
        {
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
        }

        // bind (image_type)
        //
        os << "void " << traits << "::" << endl
           << "bind (" << bind_vector << " b, image_type& i, " <<
          db << "::statement_kind sk)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (b);"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (sk);"
           << endl
           << "using namespace " << db << ";"
           << endl;

        if (readonly (c))
          os << "assert (sk != statement_update);"
             << endl;

        os << "std::size_t n (0);"
           << "ODB_POTENTIALLY_UNUSED (n);"
           << endl;

        inherits (c, bind_base_inherits_);
        names (c, bind_member_names_);

        os << "}";

        // init (image, value)
        //
        os << "bool " << traits << "::" << endl
           << "init (image_type& i, const value_type& o, " <<
          db << "::statement_kind sk)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (i);"
           << "ODB_POTENTIALLY_UNUSED (o);"
           << "ODB_POTENTIALLY_UNUSED (sk);"
           << endl
           << "using namespace " << db << ";"
           << endl;

        if (readonly (c))
          os << "assert (sk != statement_update);"
             << endl;

        os << "bool grew (false);"
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
      instance<bind_member> bind_version_member_;

      instance<init_image_base> init_image_base_;
      traversal::inherits init_image_base_inherits_;
      instance<init_image_member> init_image_member_;
      traversal::names init_image_member_names_;

      instance<init_image_member> init_id_image_member_;
      instance<init_image_member> init_version_image_member_;

      instance<init_value_base> init_value_base_;
      traversal::inherits init_value_base_inherits_;
      instance<init_value_member> init_value_member_;
      traversal::names init_value_member_names_;

      instance<init_value_member> init_id_value_member_;
      instance<init_value_member> init_version_value_member_;

      schema_emitter emitter_;
      emitter_ostream stream_;

      trav_rel::qnames drop_names_;
      instance<schema::drop_model> drop_model_;
      instance<schema::drop_table> drop_table_;
      instance<schema::drop_index> drop_index_;

      trav_rel::qnames create_names_;
      instance<schema::create_model> create_model_;
      instance<schema::create_table> create_table_;
      instance<schema::create_index> create_index_;
    };

    struct include: virtual context
    {
      typedef include base;

      virtual void
      generate ()
      {
        extra_pre ();

        os << "#include <cassert>" << endl
           << "#include <cstring> // std::memcpy" << endl
           << endl;

        os << "#include <odb/cache-traits.hxx>" << endl;

        if (embedded_schema)
          os << "#include <odb/schema-catalog-impl.hxx>" << endl;

        if (options.generate_query ())
          os << "#include <odb/details/shared-ptr.hxx>" << endl;

        os << endl;

        os << "#include <odb/" << db << "/binding.hxx>" << endl
           << "#include <odb/" << db << "/traits.hxx>" << endl
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
