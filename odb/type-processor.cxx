// file      : odb/type-processor.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <odb/type-processor.hxx>

namespace
{
  struct data_member: traversal::data_member, context
  {
    data_member (context& c)
        : context (c)
    {
      // Find the odb namespace.
      //
      tree odb = lookup_qualified_name (
        global_namespace, get_identifier ("odb"), false, false);

      if (odb == error_mark_node)
      {
        os << unit.file () << ": error: unable to resolve odb namespace"
           << endl;

        throw generation_failed ();
      }

      // Find the access class.
      //
      tree access = lookup_qualified_name (
        odb, get_identifier ("access"), true, false);

      if (access == error_mark_node)
      {
        os << unit.file () << ": error: unable to resolve access class"
           << "in the odb namespace" << endl;

        throw generation_failed ();
      }

      access = TREE_TYPE (access);

      // Find container_traits.
      //
      container_traits_ = lookup_qualified_name (
        access, get_identifier ("container_traits"), true, false);

      if (container_traits_ == error_mark_node ||
          !DECL_CLASS_TEMPLATE_P (container_traits_))
      {
        os << unit.file () << ": error: unable to resolve container_traits "
           << "in the odb namespace" << endl;

        throw generation_failed ();
      }
    }

    virtual void
    traverse (semantics::data_member& m)
    {
      if (m.count ("transient"))
        return;

      semantics::type& t (m.type ());

      // Nothing to do if this is a composite value type.
      //
      if (comp_value (t))
        return;

      string type;

      if (m.count ("type"))
        type = m.get<string> ("type");

      if (type.empty () && t.count ("type"))
        type = t.get<string> ("type");

      type = column_type_impl (t, type, &m);

      if (!type.empty ())
      {
        m.set ("column-type", type);
        return;
      }

      // See if this is a container type.
      //
      if (process_container (m))
        return;

      // If it is none of the above then we have an error.
      //
      string const& fq_type (t.fq_name (m.belongs ().hint ()));

      os << m.file () << ":" << m.line () << ":" << m.column () << ":"
         << " error: unable to map C++ type '" << fq_type << "' used in "
         << "data member '" << m.name () << "' to a database type" << endl;

      os << m.file () << ":" << m.line () << ":" << m.column () << ":"
         << " info: use '#pragma db type' to specify the database type"
         << endl;

      throw generation_failed ();
    }

    void
    process_container_value (semantics::type& t,
                             string const& prefix,
                             semantics::data_member& m)
    {
      if (comp_value (t))
        return;

      string type;

      // Custom mapping can come from these places (listed in the order
      // of priority): member, container type, value type.
      //
      if (m.count (prefix + "-type"))
        type = m.get<string> (prefix + "-type");

      semantics::type& ct (m.type ());

      if (type.empty () && ct.count (prefix + "-type"))
        type = ct.get<string> (prefix + "-type");

      if (type.empty () && t.count ("type"))
        type = t.get<string> ("type");

      type = column_type_impl (t, type, 0);

      if (!type.empty ())
      {
        m.set (prefix + "-column-type", type);
        return;
      }

      // We do not support nested containers so skip that test.
      //

      // If it is none of the above then we have an error.
      //
      string fq_type (t.fq_anonymous () ? "<anonymous>" : t.fq_name ());

      os << m.file () << ":" << m.line () << ":" << m.column () << ":"
         << " error: unable to map C++ type '" << fq_type << "' used in "
         << "data member '" << m.name () << "' to a database type" << endl;

      os << m.file () << ":" << m.line () << ":" << m.column () << ":"
         << " info: use '#pragma db " << prefix << "_type' to specify the "
         << "database type" << endl;

      throw generation_failed ();
    }

    bool
    process_container (semantics::data_member& m)
    {
      // The overall idea is as follows: try to instantiate the container
      // traits class template. If we are successeful, then this is a
      // container type and we can extract the various information from
      // the instantiation. Otherwise, this is not a container.
      //

      semantics::type& t (m.type ());

      container_kind_type ck;
      semantics::type* vt (0);
      semantics::type* it (0);
      semantics::type* kt (0);

      if (t.count ("container"))
      {
        ck = t.get<container_kind_type> ("container-kind");
        vt = t.get<semantics::type*> ("tree-value-type");

        if (ck == ck_ordered)
          it = t.get<semantics::type*> ("tree-index-type");

        if (ck == ck_map || ck == ck_multimap)
          kt = t.get<semantics::type*> ("tree-key-type");
      }
      else
      {
        tree args (make_tree_vec (1));
        TREE_VEC_ELT (args, 0) = t.tree_node ();

        // This step should succeed regardles of whether there is a
        // container traits specialization for this type.
        //
        tree inst (
          lookup_template_class (
          container_traits_, args, 0, 0, 0, tf_warning_or_error));

        if (inst == error_mark_node)
        {
          // Diagnostics has already been issued by lookup_template_class.
          //
          throw generation_failed ();
        }

        inst = TYPE_MAIN_VARIANT (inst);

        // The instantiation may already be complete if it matches a
        // (complete) specialization.
        //
        if (!COMPLETE_TYPE_P (inst))
          inst = instantiate_class_template (inst);

        // If we cannot instantiate this type, assume there is no suitable
        // container traits specialization for it.
        //
        if (inst == error_mark_node || !COMPLETE_TYPE_P (inst))
          return false;

        // @@ This points to the primary template, not the specialization.
        //
        tree decl (TYPE_NAME (inst));

        string f (DECL_SOURCE_FILE (decl));
        size_t l (DECL_SOURCE_LINE (decl));
        size_t c (DECL_SOURCE_COLUMN (decl));


        // Determine the container kind.
        //
        try
        {
          tree kind (
            lookup_qualified_name (
              inst, get_identifier ("kind"), false, false));

          if (kind == error_mark_node || TREE_CODE (kind) != VAR_DECL)
            throw generation_failed ();


          // Instantiate this decalaration so that we can get its value.
          //
          if (DECL_TEMPLATE_INSTANTIATION (kind) &&
              !DECL_TEMPLATE_INSTANTIATED (kind) &&
              !DECL_EXPLICIT_INSTANTIATION (kind))
            instantiate_decl (kind, false, false);

          tree init (DECL_INITIAL (kind));

          if (init == error_mark_node || TREE_CODE (init) != INTEGER_CST)
            throw generation_failed ();

          unsigned long long e;

          {
            HOST_WIDE_INT hwl (TREE_INT_CST_LOW (init));
            HOST_WIDE_INT hwh (TREE_INT_CST_HIGH (init));

            unsigned long long l (hwl);
            unsigned long long h (hwh);
            unsigned short width (HOST_BITS_PER_WIDE_INT);

            e = (h << width) + l;
          }

          ck = static_cast<container_kind_type> (e);
        }
        catch (generation_failed const&)
        {
          os << f << ":" << l << ":" << c << ": error: "
             << "odb::container_traits specialization does not define the "
             << "container kind constant" << endl;

          throw;
        }

        t.set ("container-kind", ck);

        // Get the value type.
        //
        try
        {
          tree decl (
            lookup_qualified_name (
              inst, get_identifier ("value_type"), true, false));

          if (decl == error_mark_node || TREE_CODE (decl) != TYPE_DECL)
            throw generation_failed ();

          tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

          vt = &dynamic_cast<semantics::type&> (*unit.find (type));
        }
        catch (generation_failed const&)
        {
          os << f << ":" << l << ":" << c << ": error: "
             << "odb::container_traits specialization does not define the "
             << "value_type type" << endl;

          throw;
        }

        t.set ("tree-value-type", vt);


        // Get the index type for ordered containers.
        //
        if (ck == ck_ordered)
        {
          try
          {
            tree decl (
              lookup_qualified_name (
                inst, get_identifier ("index_type"), true, false));

            if (decl == error_mark_node || TREE_CODE (decl) != TYPE_DECL)
              throw generation_failed ();

            tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

            it = &dynamic_cast<semantics::type&> (*unit.find (type));
          }
          catch (generation_failed const&)
          {
            os << f << ":" << l << ":" << c << ": error: "
               << "odb::container_traits specialization does not define the "
               << "index_type type" << endl;

            throw;
          }

          t.set ("tree-index-type", it);
        }

        // Get the key type for maps.
        //
        if (ck == ck_map || ck == ck_multimap)
        {
          try
          {
            tree decl (
              lookup_qualified_name (
                inst, get_identifier ("key_type"), true, false));

            if (decl == error_mark_node || TREE_CODE (decl) != TYPE_DECL)
              throw generation_failed ();

            tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

            kt = &dynamic_cast<semantics::type&> (*unit.find (type));
          }
          catch (generation_failed const&)
          {
            os << f << ":" << l << ":" << c << ": error: "
               << "odb::container_traits specialization does not define the "
               << "key_type type" << endl;

            throw;
          }

          t.set ("tree-key-type", kt);
        }
      }

      // Process member data.
      //
      process_container_value (*vt, "value", m);

      if (it != 0)
        process_container_value (*it, "index", m);

      if (kt != 0)
        process_container_value (*kt, "key", m);

      return true;
    }

  private:
    tree container_traits_;
  };

  struct class_: traversal::class_, context
  {
    class_ (context& c)
        : context (c), member_ (c)
    {
      *this >> member_names_ >> member_;
    }

    virtual void
    traverse (type& c)
    {
      bool obj (c.count ("object"));

      if (!(obj || comp_value (c)))
        return;

      names (c);

      // Assign object pointer.
      //
      if (obj)
      {
        string ptr;
        string const& name (c.fq_name ());

        if (c.count ("pointer"))
        {
          string const& p (c.get<string> ("pointer"));

          if (p == "*")
            ptr = name + "*";
          else if (p[p.size () - 1] == '*')
            ptr = p;
          else if (p.find ('<') != string::npos)
            ptr = p;
          else
          {
            // This is not a template-id. Resolve it and see if it is a
            // template or a type.
            //
            try
            {
              tree t  (resolve_type (p, c.scope ()));
              int tc (TREE_CODE (t));

              if (tc == TYPE_DECL)
                ptr = p;
              else if (tc == TEMPLATE_DECL && DECL_CLASS_TEMPLATE_P (t))
                ptr = p + "< " + name + " >";
              else
              {
                cerr << c.file () << ":" << c.line () << ":" << c.column ()
                     << ": error: name '" << p << "' specified with "
                     << "'#pragma object pointer' does not name a type "
                     << "or a template" << endl;

                throw generation_failed ();
              }
            }
            catch (invalid_name const&)
            {
              cerr << c.file () << ":" << c.line () << ":" << c.column ()
                   << ": error: type name '" << p << "' specified with "
                   << "'#pragma object pointer' is invalid" << endl;

              throw generation_failed ();
            }
            catch (unable_to_resolve const&)
            {
              cerr << c.file () << ":" << c.line () << ":" << c.column ()
                   << ": error: unable to resolve type name '" << p
                   << "' specified with '#pragma object pointer'" << endl;

              throw generation_failed ();
            }
          }
        }
        else
        {
          // Use the default pointer.
          //
          string const& p (options.default_pointer ());

          if (p == "*")
            ptr = name + "*";
          else
            ptr = p + "< " + name + " >";
        }

        c.set ("object-pointer", ptr);
      }
    }

  private:
    struct invalid_name {};
    struct unable_to_resolve {};

    tree
    resolve_type (string const& qn, semantics::scope& ss)
    {
      tree scope (ss.tree_node ());

      for (size_t b (0), e (qn.find (':')), size (qn.size ());;
           e = qn.find (':', b))
      {
        bool last (e == string::npos);
        string n (qn, b, last ? string::npos : e - b);

        if (n.empty ())
        {
          if (b == 0)
            scope = global_namespace;
          else
            throw invalid_name ();
        }
        else
        {
          tree nid (get_identifier (n.c_str ()));
          scope = lookup_qualified_name (scope, nid, last, false);

          // If this is the first component in the name, then also
          // search the outer scopes.
          //
          if (scope == error_mark_node && b == 0 && !ss.global_scope ())
          {
            semantics::scope* s (&ss);
            do
            {
              s = &s->scope_ ();
              scope = lookup_qualified_name (
                s->tree_node (), nid, last, false);
            } while (scope == error_mark_node && !s->global_scope ());
          }

          if (scope == error_mark_node)
            throw unable_to_resolve ();

          if (!last && TREE_CODE (scope) == TYPE_DECL)
            scope = TREE_TYPE (scope);
        }

        if (e == string::npos)
          break;

        if (qn[++e] != ':')
          throw invalid_name ();

        ++e; // Second ':'.

        if (e == size)
          break;

        b = e;
      }

      return scope;
    }

  private:
    data_member member_;
    traversal::names member_names_;
  };
}

void
process_types (context& ctx)
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

  unit.dispatch (ctx.unit);
}
