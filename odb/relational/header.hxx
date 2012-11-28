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
          os << "object_traits_impl< " << type << ", id_" << db <<
            " >::image_type";
        else
          os << "composite_value_traits< " << type << ", id_" << db <<
            " >::image_type";
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
          os << "base_traits::image_type* base;"
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
            os << ": access::object_traits_impl< " << type << ", id_" <<
              db << " >::" << name;
          else
            os << ": access::composite_value_traits< " << type << ", id_" <<
              db << " >::" << public_name (m) << "_traits"; // No prefix_.
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
            query_columns_type_ (false, true, false),
            pointer_query_columns_type_ (true, true, false)
      {
      }

      class1 (class_ const&)
          : root_context (), //@@ -Wextra
            context (),
            id_image_member_ ("id_"),
            version_image_member_ ("version_"),
            discriminator_image_member_ ("discriminator_"),
            query_columns_type_ (false, true, false),
            pointer_query_columns_type_ (true, true, false)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (!options.at_once () && class_file (c) != unit.file ())
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
      traverse_object (type&);

      virtual void
      view_public_extra_pre (type&)
      {
      }

      virtual void
      view_public_extra_post (type&)
      {
      }

      virtual void
      traverse_view (type&);

      virtual void
      traverse_composite (type&);

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

      class2 ()
          : query_columns_type_ (false, true, false),
            query_columns_type_inst_ (false, false, true),
            view_query_columns_type_ (true)
      {
      }

      class2 (class_ const&)
          : root_context (), //@@ -Wextra
            context (),
            query_columns_type_ (false, true, false),
            query_columns_type_inst_ (false, false, true),
            view_query_columns_type_ (true)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (!options.at_once () && class_file (c) != unit.file ())
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
        if (options.generate_query ())
        {
          os << "// " << class_name (c) << endl
             << "//" << endl;

          // query_columns
          //
          // If we don't have any pointers, then query_columns is generated
          // in pass 1 (see the comment in class1 for details).
          //
          if (has_a (c, test_pointer | include_base))
            query_columns_type_->traverse (c);

          // Generate extern template declarations.
          //
          if (multi_dynamic)
            query_columns_type_inst_->traverse (c);
        }

        // Move header comment out of if-block if adding any code here.
      }

      virtual void
      traverse_view (type& c)
      {
        // query_columns
        //
        if (c.get<size_t> ("object-count") != 0)
        {
          os << "// " << class_name (c) << endl
             << "//" << endl;

          view_query_columns_type_->traverse (c);
        }

        // Move header comment out of if-block if adding any code here.
      }

      virtual void
      traverse_composite (type&)
      {
      }

    private:
      instance<query_columns_type> query_columns_type_;
      instance<query_columns_type> query_columns_type_inst_;
      instance<view_query_columns_type> view_query_columns_type_;
    };

    struct include: virtual context
    {
      typedef include base;

      virtual void
      generate ()
      {
        os << "#include <odb/details/buffer.hxx>" << endl
           << endl;

        os << "#include <odb/" << db << "/version.hxx>" << endl
           << "#include <odb/" << db << "/forward.hxx>" << endl
           << "#include <odb/" << db << "/binding.hxx>" << endl
           << "#include <odb/" << db << "/" << db << "-types.hxx>" << endl;

        if (options.generate_query ())
        {
          os << "#include <odb/" << db << "/query.hxx>" << endl;

          if (multi_dynamic)
            os << "#include <odb/" << db << "/query-dynamic.hxx>" << endl;
        }

        os << endl;
      }
    };
  }
}

#endif // ODB_RELATIONAL_HEADER_HXX
