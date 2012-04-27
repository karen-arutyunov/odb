// file      : odb/relational/header.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_HEADER_HXX
#define ODB_RELATIONAL_HEADER_HXX

#include <odb/relational/context.hxx>
#include <odb/relational/common.hxx>

namespace relational
{
  namespace header
  {
    //
    // image_type
    //

    struct image_member: virtual member_base
    {
      typedef image_member base;

      image_member (string const& var = string ())
          : member_base (var, 0, string (), string ())
      {
      }

      image_member (string const& var,
                    semantics::type& t,
                    string const& fq_type,
                    string const& key_prefix)
          : member_base (var, &t, fq_type, key_prefix)
      {
      }
    };

    struct image_base: traversal::class_, virtual context
    {
      typedef image_base base;

      image_base (): first_ (true) {}

      virtual void
      traverse (type& c)
      {
        bool obj (object (c));

        // Ignore transient bases. Not used for views.
        //
        if (!(obj || composite (c)))
          return;

        if (first_)
        {
          os << ": ";
          first_ = false;
        }
        else
        {
          os << "," << endl
             << "  ";
        }

        string const& type (class_fq_name (c));

        if (obj)
          os << "object_traits< " << type << " >::image_type";
        else
          os << "composite_value_traits< " << type << " >::image_type";
      }

    private:
      bool first_;
    };

    struct image_type: traversal::class_, virtual context
    {
      typedef image_type base;

      image_type ()
      {
        *this >> names_member_ >> member_;
      }

      image_type (image_type const&)
          : root_context (), context () //@@ -Wextra
      {
        *this >> names_member_ >> member_;
      }

      virtual void
      image_extra (type&)
      {
      }

      virtual void
      traverse (type& c)
      {
        type* poly_root (polymorphic (c));
        bool poly_derived (poly_root != 0 && poly_root != &c);

        os << "struct image_type";

        if (!view (c))
        {
          // Don't go into the base if we are a derived type in a
          // polymorphic hierarchy.
          //
          if (!poly_derived)
          {
            instance<image_base> b;
            traversal::inherits i (*b);
            inherits (c, i);
          }
        }

        os << "{";

        if (poly_derived)
          os << "object_traits<base_type>::image_type* base;"
             << endl;

        names (c);

        // We don't need a version if this is a composite value type
        // or reuse-abstract object.
        //
        if (!(composite (c) || (abstract (c) && !polymorphic (c))))
          os << "std::size_t version;"
             << endl;

        image_extra (c);

        os << "};";
      }

    private:
      instance<image_member> member_;
      traversal::names names_member_;
    };

    //
    // query_columns_type
    //

    struct query_columns_bases: traversal::class_, virtual context
    {
      typedef query_columns_bases base;

      query_columns_bases (bool ptr, bool first = true)
          : ptr_ (ptr), first_ (first)
      {
      }

      virtual void
      traverse (type& c)
      {
        // Ignore transient bases. Not used for views.
        //
        if (!object (c))
          return;

        if (first_)
        {
          os << ":" << endl
             << "  ";
          first_ = false;
        }
        else
        {
          os << "," << endl
             << "  ";
        }

        os << (ptr_ ? "pointer_query_columns" : "query_columns") << "< " <<
          class_fq_name (c) << ", ";

        // If our base is polymorphic, then it has its own table/alias.
        //
        if (polymorphic (c))
          os << "typename A::base_traits";
        else
          os << "A";

        os << " >";
      }

    private:
      bool ptr_;
      bool first_;
    };

    struct query_columns_base_aliases: traversal::class_, virtual context
    {
      typedef query_columns_base_aliases base;

      query_columns_base_aliases (bool ptr)
          : ptr_ (ptr)
      {
      }

      virtual void
      traverse (type& c)
      {
        // Ignore transient bases. Not used for views.
        //
        if (!object (c))
          return;

        string const& name (class_name (c));

        os  << "// " << name << endl
            << "//" << endl
            << "typedef " <<
          (ptr_ ? "pointer_query_columns" : "query_columns") << "< " <<
          class_fq_name (c) << ", ";

        if (polymorphic (c))
          os << "typename A::base_traits";
        else
          os << "A";

        os << " > " << name << ";"
           << endl;
      }

    private:
      bool ptr_;
    };

    struct query_columns_type: traversal::class_, virtual context
    {
      typedef query_columns_type base;

      // Depending on the ptr argument, generate query_columns or
      // pointer_query_columns specialization. The latter is used
      // for object pointers where we don't support nested pointers.
      //
      query_columns_type (bool ptr): ptr_ (ptr) {}

      virtual void
      traverse (type& c)
      {
        string const& type (class_fq_name (c));

        if (ptr_)
        {
          os << "template <typename A>" << endl
             << "struct pointer_query_columns< " << type << ", A >";

          // If we don't have pointers (in the whole hierarchy), then
          // pointer_query_columns and query_columns are the same.
          //
          if (!has_a (c, test_pointer | include_base))
          {
            os << ":" << endl
               << "  query_columns< " << type << ", A >"
               << "{"
               << "};";
          }
          else
          {
            {
              instance<query_columns_bases> b (ptr_);
              traversal::inherits i (*b);
              inherits (c, i);
            }

            os << "{";

            {
              instance<query_columns_base_aliases> b (ptr_);
              traversal::inherits i (*b);
              inherits (c, i);
            }

            {
              instance<query_columns> t (ptr_);
              t->traverse (c);
            }

            os << "};";

            {
              instance<query_columns> t (ptr_, c);
              t->traverse (c);
            }
          }
        }
        else
        {
          bool has_ptr (has_a (c, test_pointer | exclude_base));

          if (has_ptr)
          {
            // This class contains everything for inverse pointers and
            // aliases for non-inverse ones. It doesn't depend on the
            // table alias (A) template argument.
            //
            os << "template <>" << endl
               << "struct query_columns_base< " << type << " >"
               << "{";

            instance<query_columns_base> t;
            t->traverse (c);

            os << "};";
          }

          os << "template <typename A>" << endl
             << "struct query_columns< " << type << ", A >";

          if (has_ptr)
            os << ":" << endl
               << "  query_columns_base< " << type << " >";

          {
            instance<query_columns_bases> b (ptr_, !has_ptr);
            traversal::inherits i (*b);
            inherits (c, i);
          }

          os << "{";

          {
            instance<query_columns_base_aliases> b (ptr_);
            traversal::inherits i (*b);
            inherits (c, i);
          }

          {
            instance<query_columns> t (ptr_);
            t->traverse (c);
          }

          os << "};";

          {
            instance<query_columns> t (ptr_, c);
            t->traverse (c);
          }
        }
      }

    public:
      bool ptr_;
    };

    // Member-specific traits types for container members.
    //
    struct container_traits: object_members_base, virtual context
    {
      typedef container_traits base;

      container_traits (semantics::class_& c)
          : object_members_base (true, false, false), c_ (c)
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
      container_public_extra_pre (semantics::data_member&)
      {
      }

      virtual void
      container_public_extra_post (semantics::data_member&)
      {
      }

      virtual void
      traverse_container (semantics::data_member& m, semantics::type& c)
      {
        using semantics::type;
        using semantics::class_;

        // Figure out if this member is from a base object or composite
        // value and if it's from an object, whether it is reuse-abstract.
        //
        bool base, reuse_abst;

        if (object (c_))
        {
          base = cur_object != &c_ ||
            !object (dynamic_cast<type&> (m.scope ()));
          reuse_abst = abstract (c_) && !polymorphic (c_);
        }
        else
        {
          base = false;      // We don't go into bases.
          reuse_abst = true; // Always abstract.
        }

        container_kind_type ck (container_kind (c));

        type& vt (container_vt (c));
        type* it (0);
        type* kt (0);

        bool ordered (false);
        bool inverse (context::inverse (m, "value"));

        switch (ck)
        {
        case ck_ordered:
          {
            if (!unordered (m))
            {
              it = &container_it (c);
              ordered = true;
            }
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            kt = &container_kt (c);
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        string name (flat_prefix_ + public_name (m) + "_traits");

        // Figure out column counts.
        //
        size_t id_columns, data_columns, cond_columns;

        if (!reuse_abst)
        {
          type& idt (container_idt (m));

          if (class_* idc = composite_wrapper (idt))
            id_columns = column_count (*idc).total;
          else
            id_columns = 1;

          data_columns = cond_columns = id_columns;

          switch (ck)
          {
          case ck_ordered:
            {
              // Add one for the index.
              //
              if (ordered)
              {
                data_columns++;

                // Index is not currently used (see also bind()).
                //
                // cond_columns++;
              }
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              // Add some for the key.
              //
              size_t n;

              class_* ptr (object_pointer (*kt));
              semantics::type& t (ptr == 0 ? *kt : utype (*id_member (*ptr)));

              if (class_* comp = composite_wrapper (t))
                n = column_count (*comp).total;
              else
                n = 1;

              data_columns += n;

              // Key is not currently used (see also bind()).
              //
              // cond_columns += n;

              break;
            }
          case ck_set:
          case ck_multiset:
            {
              // Not currently used (see also bind())
              //
              // Value is also a key.
              //
              // class_* ptr (object_pointer (vt));
              // semantics::type& t (ptr == 0 ? vt : utype (*id_member (*ptr)));
              //
              // if (class_* comp = composite_wrapper (t))
              //   cond_columns += column_count (*comp).total;
              // else
              //   cond_columns++;
              //
              break;
            }
          }

          {
            class_* ptr (object_pointer (vt));
            semantics::type& t (ptr == 0 ? vt : utype (*id_member (*ptr)));

            if (class_* comp = composite_wrapper (t))
              data_columns += column_count (*comp).total;
            else
              data_columns++;
          }

          // Store column counts for the source generator.
          //
          m.set ("id-column-count", id_columns);
          m.set ("cond-column-count", cond_columns);
          m.set ("data-column-count", data_columns);
        }

        os << "// " << m.name () << endl
           << "//" << endl
           << "struct " << name;

        if (base)
        {
          semantics::class_& b (dynamic_cast<semantics::class_&> (m.scope ()));
          string const& type (class_fq_name (b));

          if (object (b))
            os << ": access::object_traits< " << type << " >::" << name;
          else
            os << ": access::composite_value_traits< " << type << " >::" <<
              public_name (m) << "_traits"; // No prefix_.
        }

        os << "{";

        container_public_extra_pre (m);

        if (!reuse_abst)
        {
          // column_count
          //
          os << "static const std::size_t id_column_count = " <<
            id_columns << "UL;"
             << "static const std::size_t cond_column_count = " <<
            cond_columns << "UL;"
             << "static const std::size_t data_column_count = " <<
            data_columns << "UL;"
             << endl;

          // Statements.
          //
          os << "static const char insert_one_statement[];"
             << "static const char select_all_statement[];"
             << "static const char delete_all_statement[];"
             << endl;
        }

        if (base)
        {
          container_public_extra_post (m);
          os << "};";

          return;
        }

        // container_type
        // container_traits
        // index_type
        // key_type
        // value_type
        //
        os << "typedef ";

        {
          semantics::names* hint;
          semantics::type& t (utype (m, hint));

          if (semantics::type* wt = wrapper (t))
          {
            // Use the hint from the wrapper unless the wrapped type is
            // qualified. In this case use the hint for the unqualified
            // type.
            //
            hint = t.get<semantics::names*> ("wrapper-hint");
            utype (*wt, hint);

            os << c.fq_name (hint);
          }
          else
            // t and c are the same.
            //
            os << t.fq_name (hint);
        }

        os << " container_type;";

        os << "typedef" << endl
           << "odb::access::container_traits<container_type>" << endl
           << "container_traits_type;";

        switch (ck)
        {
        case ck_ordered:
          {
            os << "typedef container_traits_type::index_type index_type;";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "typedef container_traits_type::key_type key_type;";
          }
        case ck_set:
        case ck_multiset:
          {
            break;
          }
        }

        os << "typedef container_traits_type::value_type value_type;"
           << endl;

        // functions_type
        //
        switch (ck)
        {
        case ck_ordered:
          {
            os << "typedef ordered_functions<index_type, value_type> " <<
              "functions_type;";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "typedef map_functions<key_type, value_type> " <<
              "functions_type;";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "typedef set_functions<value_type> functions_type;";
            break;
          }
        }

        os << "typedef " << db << "::container_statements< " << name <<
          " > statements_type;"
           << endl;

        // cond_image_type (object id is taken from the object image)
        //
        os << "struct cond_image_type"
           << "{";

        switch (ck)
        {
        case ck_ordered:
          {
            if (ordered)
            {
              os << "// index" << endl
                 << "//" << endl;
              instance<image_member> im ("index_", *it, "index_type", "index");
              im->traverse (m);
            }
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "// key" << endl
               << "//" << endl;
            instance<image_member> im ("key_", *kt, "key_type", "key");
            im->traverse (m);
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "// value" << endl
               << "//" << endl;
            instance<image_member> im ("value_", vt, "value_type", "value");
            im->traverse (m);
            break;
          }
        }

        os << "std::size_t version;"
           << "};";

        // data_image_type (object id is taken from the object image)
        //
        os << "struct data_image_type"
           << "{";

        switch (ck)
        {
        case ck_ordered:
          {
            if (ordered)
            {
              os << "// index" << endl
                 << "//" << endl;
              instance<image_member> im ("index_", *it, "index_type", "index");
              im->traverse (m);
            }
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "// key" << endl
               << "//" << endl;
            instance<image_member> im ("key_", *kt, "key_type", "key");
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
        instance<image_member> im ("value_", vt, "value_type", "value");
        im->traverse (m);

        os << "std::size_t version;"
           << "};";

        // bind (cond_image)
        //
        os << "static void" << endl
           << "bind (" << bind_vector << "," << endl
           << "const " << bind_vector << " id," << endl
           << "std::size_t id_size," << endl
           << "cond_image_type&);"
           << endl;

        // bind (data_image)
        //
        os << "static void" << endl
           << "bind (" << bind_vector << "," << endl
           << "const " << bind_vector << " id," << endl
           << "std::size_t id_size," << endl
           << "data_image_type&);"
           << endl;

        // grow ()
        //
        if (generate_grow)
        {
          os << "static void" << endl
             << "grow (data_image_type&, " << truncated_vector << ");"
             << endl;
        }

        // init (data_image)
        //
        if (!inverse)
        {
          os << "static void" << endl;

          switch (ck)
          {
          case ck_ordered:
            {
              if (ordered)
                os << "init (data_image_type&, index_type, const value_type&);";
              else
                os << "init (data_image_type&, const value_type&);";
              break;
            }
          case ck_map:
          case ck_multimap:
            {
              os << "init (data_image_type&, const key_type&, const value_type&);";
              break;
            }
          case ck_set:
          case ck_multiset:
            {
              os << "init (data_image_type&, const value_type&);";
              break;
            }
          }

          os << endl;
        }

        // init (data)
        //
        os << "static void" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            if (ordered)
              os << "init (index_type&, value_type&, ";
            else
              os << "init (value_type&, ";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "init (key_type&, value_type&, ";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "init (value_type&, ";
            break;
          }
        }

        os << "const data_image_type&, database*);"
           << endl;

        // insert_one
        //
        os << "static void" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "insert_one (index_type, const value_type&, void*);";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "insert_one (const key_type&, const value_type&, void*);";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "insert_one (const value_type&, void*);";
            break;
          }
        }

        os << endl;

        // load_all
        //
        os << "static bool" << endl;

        switch (ck)
        {
        case ck_ordered:
          {
            os << "load_all (index_type&, value_type&, void*);";
            break;
          }
        case ck_map:
        case ck_multimap:
          {
            os << "load_all (key_type&, value_type&, void*);";
            break;
          }
        case ck_set:
        case ck_multiset:
          {
            os << "load_all (value_type&, void*);";
            break;
          }
        }

        os << endl;

        // delete_all
        //
        os << "static void" << endl
           << "delete_all (void*);"
           << endl;

        // persist
        //
        if (!inverse)
          os << "static void" << endl
             << "persist (const container_type&," << endl
             << "const " << db << "::binding& id," << endl
             << "statements_type&);"
             << endl;

        // load
        //
        os << "static void" << endl
           << "load (container_type&," << endl
           << "const " << db << "::binding& id," << endl
           << "statements_type&);"
           << endl;

        // update
        //
        if (!(inverse || readonly (member_path_, member_scope_)))
          os << "static void" << endl
             << "update (const container_type&," << endl
             << "const " << db << "::binding& id," << endl
             << "statements_type&);"
             << endl;

        // erase
        //
        if (!inverse)
          os << "static void" << endl
             << "erase (const " << db << "::binding& id, statements_type&);"
             << endl;

        container_public_extra_post (m);

        os << "};";
      }

    protected:
      semantics::class_& c_;
    };

    // First pass over objects, views, and composites. Some code must be
    // split into two parts to deal with yet undefined types.
    //
    struct class1: traversal::class_, virtual context
    {
      typedef class1 base;

      class1 ()
          : id_image_member_ ("id_"),
            version_image_member_ ("version_"),
            discriminator_image_member_ ("discriminator_"),
            query_columns_type_ (false),
            pointer_query_columns_type_ (true)
      {
      }

      class1 (class_ const&)
          : root_context (), //@@ -Wextra
            context (),
            id_image_member_ ("id_"),
            version_image_member_ ("version_"),
            discriminator_image_member_ ("discriminator_"),
            query_columns_type_ (false),
            pointer_query_columns_type_ (true)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (class_file (c) != unit.file ())
          return;

        if (object (c))
          traverse_object (c);
        else if (view (c))
          traverse_view (c);
        else if (composite (c))
          traverse_composite (c);
      }

      virtual void
      object_public_extra_pre (type&)
      {
      }

      virtual void
      object_public_extra_post (type&)
      {
      }

      virtual void
      traverse_object (type& c);

      virtual void
      view_public_extra_pre (type&)
      {
      }

      virtual void
      view_public_extra_post (type&)
      {
      }

      virtual void
      traverse_view (type& c)
      {
        string const& type (class_fq_name (c));

        os << "// " << class_name (c) << endl
           << "//" << endl;

        os << "template <>" << endl
           << "struct class_traits< " << type << " >"
           << "{"
           << "static const class_kind kind = class_view;"
           << "};";

        os << "template <>" << endl
           << "class access::view_traits< " << type << " >"
           << "{"
           << "public:" << endl;

        view_public_extra_pre (c);

        // view_type & pointer_type
        //
        os << "typedef " << type << " view_type;"
           << "typedef " << c.get<string> ("object-pointer") << " pointer_type;"
           << endl;

        // image_type
        //
        image_type_->traverse (c);

        //
        // Query.
        //

        // query_base_type
        //
        os << "typedef " << db << "::query query_base_type;"
           << endl;

        // query_type (definition generated by class2).
        //
        size_t obj_count (c.get<size_t> ("object-count"));

        if (obj_count != 0)
        {
          if (obj_count > 1)
            os << "struct query_columns;";

          os << "struct query_type;"
             << endl;
        }
        else
          os << "typedef query_base_type query_type;"
             << endl;

        //
        // Functions.
        //

        // grow ()
        //
        if (generate_grow)
        {
          os << "static bool" << endl
             << "grow (image_type&, " << truncated_vector << ");"
             << endl;
        }

        // bind (image_type)
        //
        os << "static void" << endl
           << "bind (" << bind_vector << ", image_type&);"
           << endl;

        // init (view, image)
        //
        os << "static void" << endl
           << "init (view_type&, const image_type&, database*);"
           << endl;

        // column_count
        //
        os << "static const std::size_t column_count = " <<
          column_count (c).total << "UL;"
           << endl;

        // Statements.
        //
        view_query& vq (c.get<view_query> ("query"));

        if (vq.kind != view_query::runtime)
        {
          os << "static query_base_type" << endl
             << "query_statement (const query_base_type&);"
             << endl;
        }

        //
        // Functions.
        //

        // callback ()
        //
        os << "static void" << endl
           << "callback (database&, view_type&, callback_event);"
           <<  endl;

        // query ()
        //
        os << "static result<view_type>" << endl
           << "query (database&, const query_base_type&);"
           << endl;

        view_public_extra_post (c);

        os << "};";
      }

      virtual void
      traverse_composite (type& c)
      {
        string const& type (class_fq_name (c));

        os << "// " << class_name (c) << endl
           << "//" << endl;

        os << "template <>" << endl
           << "struct class_traits< " << type << " >"
           << "{"
           << "static const class_kind kind = class_composite;"
           << "};";

        os << "template <>" << endl
           << "class access::composite_value_traits< " << type << " >"
           << "{"
           << "public:" << endl;

        // value_type
        //
        os << "typedef " << type << " value_type;"
           << endl;

        // image_type
        //
        image_type_->traverse (c);

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
          os << "static bool" << endl
             << "grow (image_type&, " << truncated_vector << ");"
             << endl;
        }

        // bind (image_type)
        //
        os << "static void" << endl
           << "bind (" << bind_vector << ", image_type&, " <<
          db << "::statement_kind);"
           << endl;

        // init (image, value)
        //
        os << "static bool" << endl
           << "init (image_type&, const value_type&, " <<
          db << "::statement_kind);"
           << endl;

        // init (value, image)
        //
        os << "static void" << endl
           << "init (value_type&, const image_type&, database*);"
           << endl;

        if (!has_a (c, test_container))
        {
          // get_null (image)
          //
          os << "static bool" << endl
             << "get_null (const image_type&);"
             << endl;

          // set_null (image)
          //
          os << "static void" << endl
             << "set_null (image_type&, " << db << "::statement_kind);"
             << endl;
        }

        os << "};";
      }

    private:
      instance<image_type> image_type_;
      instance<image_member> id_image_member_;
      instance<image_member> version_image_member_;
      instance<image_member> discriminator_image_member_;

      instance<query_columns_type> query_columns_type_;
      instance<query_columns_type> pointer_query_columns_type_;
    };

    // Second pass over objects, views, and composites.
    //
    struct class2: traversal::class_, virtual context
    {
      typedef class2 base;

      class2 (): query_columns_type_ (false) {}

      class2 (class_ const&)
          : root_context (), //@@ -Wextra
            context (),
            query_columns_type_ (false)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (class_file (c) != unit.file ())
          return;

        if (object (c))
          traverse_object (c);
        else if (view (c))
          traverse_view (c);
        else if (composite (c))
          traverse_composite (c);
      }

      virtual void
      traverse_object (type& c)
      {
        bool reuse_abst (abstract (c) && !polymorphic (c));

        string const& type (class_fq_name (c));

        if (options.generate_query ())
        {
          bool has_ptr (has_a (c, test_pointer | include_base));

          if (has_ptr || !reuse_abst)
            os << "// " << class_name (c) << endl
               << "//" << endl;

          // query_columns
          //
          // If we don't have any pointers, then query_columns is generated
          // in pass 1 (see the comment in class1 for details).
          //
          if (has_ptr)
          {
            instance<query_alias_traits> t (alias_tags_, alias_specs_);
            t->traverse (c);

            query_columns_type_->traverse (c);
          }

          // query_type
          //
          if (!reuse_abst)
            os << "struct access::object_traits< " << type << " >::" <<
              "query_type:" << endl
               << "  query_base_type," << endl
               << "  query_columns<" << endl
               << "    " << type << "," << endl
               << "    access::object_traits< " << type << "> >"
               << "{"
               << "query_type ();"
               << "query_type (bool);"
               << "query_type (const char*);"
               << "query_type (const std::string&);"
               << "query_type (const query_base_type&);"
               << "};";
        }

        // Move header comment out of if-block if adding any code here.
      }

      virtual void
      traverse_view (type& c)
      {
        string const& type (class_fq_name (c));

        // query_type
        //
        size_t obj_count (c.get<size_t> ("object-count"));

        if (obj_count != 0)
        {
          os << "// " << class_name (c) << endl
             << "//" << endl;

          view_objects& objs (c.get<view_objects> ("objects"));

          // Generate the alias tags and alias_traits specializations.
          //
          {
            instance<query_alias_traits> at (alias_tags_, alias_specs_);

            for (view_objects::const_iterator i (objs.begin ());
                 i < objs.end ();
                 ++i)
            {
              if (i->kind != view_object::object)
                continue; // Skip tables.

              if (i->alias.empty ())
                continue;

              semantics::class_& o (*i->obj);
              qname const& t (table_name (o));

              // Check that the alias is not the same as the table name
              // (if this is a polymorphic object, then the alias is just
              // a prefix).
              //
              if (polymorphic (o) || t.qualified () || i->alias != t.uname ())
                at->generate (i->alias, o);
            }
          }

          if (obj_count > 1)
          {
            os << "struct access::view_traits< " << type << " >::query_columns"
               << "{";

            for (view_objects::const_iterator i (objs.begin ());
                 i < objs.end ();
                 ++i)
            {
              if (i->kind != view_object::object)
                continue; // Skip tables.

              bool alias (!i->alias.empty ());
              semantics::class_& o (*i->obj);
              string const& oname (alias ? i->alias : class_name (o));
              string const& otype (class_fq_name (o));
              qname const& table (table_name (o));

              os << "// " << oname << endl
                 << "//" << endl
                 << "typedef" << endl
                 << "odb::pointer_query_columns<" << endl
                 << "  " << otype << "," << endl;

              if (alias && (polymorphic (o) ||
                            table.qualified () ||
                            i->alias != table.uname ()))
              {
                string tag (escape (i->alias + "_alias_tag"));
                os << "  odb::alias_traits< " << otype << ", " <<
                  tag << " > >" << endl;
              }
              else
                os << "  odb::access::object_traits< " << otype <<
                  " > >" << endl;

              os << oname << ";"
                 << endl;
            }

            os << "};";

            os << "struct access::view_traits< " << type << " >::" <<
              "query_type:" << endl
               << "  query_base_type, query_columns";
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

            bool alias (!vo->alias.empty ());
            semantics::class_& o (*vo->obj);
            string const& otype (class_fq_name (o));
            qname const& table (table_name (o));

            os << "struct access::view_traits< " << type << " >::" <<
              "query_type:" << endl
               << "  query_base_type," << endl
               << "  odb::pointer_query_columns<" << endl
               << "    " << otype << "," << endl;

            if (alias && (polymorphic (o) ||
                          table.qualified () ||
                          vo->alias != table.uname ()))
            {
              string tag (escape (vo->alias + "_alias_tag"));
              os << "    odb::alias_traits< " << otype << ", " <<
                tag << " > >";
            }
            else
              os << "    odb::access::object_traits< " << otype << " > >";
          }

          os << "{"
             << "query_type ();"
             << "query_type (bool);"
             << "query_type (const char*);"
             << "query_type (const std::string&);"
             << "query_type (const query_base_type&);"
             << "};";
        }

        // Move header comment out of if-block if adding any code here.
      }

      virtual void
      traverse_composite (type&)
      {
      }

    private:
      instance<query_columns_type> query_columns_type_;

      std::set<string> alias_tags_;
      std::set<string> alias_specs_;
    };

    struct include: virtual context
    {
      typedef include base;

      virtual void
      generate ()
      {
        os << "#include <odb/details/buffer.hxx>" << endl
           << "#include <odb/details/unused.hxx>" << endl
           << endl;

        os << "#include <odb/" << db << "/version.hxx>" << endl
           << "#include <odb/" << db << "/forward.hxx>" << endl
           << "#include <odb/" << db << "/binding.hxx>" << endl
           << "#include <odb/" << db << "/" << db << "-types.hxx>" << endl;

        if (options.generate_query ())
          os << "#include <odb/" << db << "/query.hxx>" << endl;

        os << endl;
      }
    };
  }
}

#endif // ODB_RELATIONAL_HEADER_HXX
