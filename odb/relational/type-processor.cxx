// file      : odb/relational/type-processor.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <odb/cxx-lexer.hxx>

#include <odb/relational/context.hxx>
#include <odb/relational/type-processor.hxx>

using namespace std;

namespace relational
{
  namespace
  {
    // Indirect (dynamic) context values.
    //
    static semantics::type*
    id_tree_type ()
    {
      context& c (context::current ());
      semantics::data_member& id (*context::id_member (*c.top_object));
      return &id.type ();
    }

    static string
    id_column_type ()
    {
      context& c (context::current ());
      semantics::data_member& id (*context::id_member (*c.top_object));
      return id.get<string> ("column-type");
    }

    struct data_member: traversal::data_member, context
    {
      data_member ()
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

        // Find pointer traits.
        //
        pointer_traits_ = lookup_qualified_name (
          odb, get_identifier ("pointer_traits"), true, false);

        if (container_traits_ == error_mark_node ||
            !DECL_CLASS_TEMPLATE_P (pointer_traits_))
        {
          os << unit.file () << ": error: unable to resolve pointer_traits "
             << "in the odb namespace" << endl;

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

        string type, ref_type;

        if (m.count ("type"))
          type = m.get<string> ("type");

        if (semantics::class_* c = process_object_pointer (m, t))
        {
          // This is an object pointer. The column type is the pointed-to
          // object id type. Except by default it can be NULL.
          //
          semantics::data_member& id (*id_member (*c));
          semantics::type& idt (id.type ());

          if (type.empty () && id.count ("type"))
            type = id.get<string> ("type");

          if (type.empty () && idt.count ("id-type"))
            type = idt.get<string> ("id-type");

          if (type.empty () && idt.count ("type"))
            type = idt.get<string> ("type");

          column_type_flags f (ctf_none);

          if (null_pointer (m))
            f |= ctf_default_null;

          type = database_type (idt, type, id, f);
        }
        else
        {
          if (type.empty () && m.count ("id") && t.count ("id-type"))
            type = t.get<string> ("id-type");

          if (type.empty () && t.count ("type"))
            type = t.get<string> ("type");

          type = database_type (t, type, m, ctf_none);
        }

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
                               semantics::data_member& m,
                               string const& prefix,
                               bool obj_ptr)
      {
        if (comp_value (t))
          return;

        string type;
        semantics::type& ct (m.type ());

        // Custom mapping can come from these places (listed in the order
        // of priority): member, container type, value type. To complicate
        // things a bit, for object references, it can also come from the
        // member and value type of the id member.
        //
        if (m.count (prefix + "-type"))
          type = m.get<string> (prefix + "-type");

        if (type.empty () && ct.count (prefix + "-type"))
          type = ct.get<string> (prefix + "-type");

        semantics::class_* c;
        if (obj_ptr && (c = process_object_pointer (m, t, prefix)))
        {
          // This is an object pointer. The column type is the pointed-to
          // object id type. Except by default it can be NULL.
          //
          semantics::data_member& id (*id_member (*c));
          semantics::type& idt (id.type ());

          if (type.empty () && id.count ("type"))
            type = id.get<string> ("type");

          if (type.empty () && idt.count ("id-type"))
            type = idt.get<string> ("id-type");

          if (type.empty () && idt.count ("type"))
            type = idt.get<string> ("type");

          column_type_flags f (ctf_none);

          if (null_pointer (m, prefix))
            f |= ctf_default_null;

          type = database_type (idt, type, id, f);
        }
        else
        {
          if (type.empty () && t.count ("type"))
            type = t.get<string> ("type");

          type = database_type (t, type, m, ctf_none);
        }

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
          vt = t.get<semantics::type*> ("value-tree-type");

          if (ck == ck_ordered)
            it = t.get<semantics::type*> ("index-tree-type");

          if (ck == ck_map || ck == ck_multimap)
            kt = t.get<semantics::type*> ("key-tree-type");
        }
        else
        {
          tree inst (instantiate_template (container_traits_, t.tree_node ()));

          if (inst == 0)
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
               << "container_traits specialization does not define the "
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
               << "container_traits specialization does not define the "
               << "value_type type" << endl;

            throw;
          }

          t.set ("value-tree-type", vt);


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
                 << "container_traits specialization does not define the "
                 << "index_type type" << endl;

              throw;
            }

            t.set ("index-tree-type", it);
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
                 << "container_traits specialization does not define the "
                 << "key_type type" << endl;

              throw;
            }

            t.set ("key-tree-type", kt);
          }
        }

        // Process member data.
        //
        m.set ("id-tree-type", &id_tree_type);
        m.set ("id-column-type", &id_column_type);

        process_container_value (*vt, m, "value", true);

        if (it != 0)
          process_container_value (*it, m, "index", false);

        if (kt != 0)
          process_container_value (*kt, m, "key", false);

        // If this is an inverse side of a bidirectional object relationship
        // and it is an ordered container, mark it as unordred since there is
        // no concept of order in this construct.
        //
        if (ck == ck_ordered && m.count ("value-inverse"))
          m.set ("unordered", string ()); // Keep compatible with pragma.

        return true;
      }

      semantics::class_*
      process_object_pointer (semantics::data_member& m,
                              semantics::type& t,
                              string const& kp = string ())
      {
        // The overall idea is as follows: try to instantiate the pointer
        // traits class template. If we are successeful, then get the
        // element type and see if it is an object.
        //
        using semantics::class_;
        using semantics::data_member;

        class_* c (0);

        if (t.count ("element-type"))
          c = t.get<class_*> ("element-type");
        else
        {
          tree inst (instantiate_template (pointer_traits_, t.tree_node ()));

          if (inst == 0)
            return 0;

          // @@ This points to the primary template, not the specialization.
          //
          tree decl (TYPE_NAME (inst));

          string fl (DECL_SOURCE_FILE (decl));
          size_t ln (DECL_SOURCE_LINE (decl));
          size_t cl (DECL_SOURCE_COLUMN (decl));

          // Get the element type.
          //
          tree tn (0);
          try
          {
            tree decl (
              lookup_qualified_name (
                inst, get_identifier ("element_type"), true, false));

            if (decl == error_mark_node || TREE_CODE (decl) != TYPE_DECL)
              throw generation_failed ();

            tn = TYPE_MAIN_VARIANT (TREE_TYPE (decl));

            // Check if the pointer is a TR1 template instantiation.
            //
            if (tree ti = TYPE_TEMPLATE_INFO (t.tree_node ()))
            {
              decl = TI_TEMPLATE (ti); // DECL_TEMPLATE

              // Get to the most general template declaration.
              //
              while (DECL_TEMPLATE_INFO (decl))
                decl = DECL_TI_TEMPLATE (decl);

              if (!unit.count ("tr1-pointer-used"))
              {
                unit.set ("tr1-pointer-used", false);
                unit.set ("boost-pointer-used", false);
              }

              bool& tr1 (unit.get<bool> ("tr1-pointer-used"));
              bool& boost (unit.get<bool> ("boost-pointer-used"));

              string n (decl_as_string (decl, TFF_PLAIN_IDENTIFIER));

              // In case of a boost TR1 implementation, we cannot distinguish
              // between the boost:: and std::tr1:: usage since the latter is
              // just a using-declaration for the former.
              //
              tr1 = tr1
                || n.compare (0, 8, "std::tr1") == 0
                || n.compare (0, 10, "::std::tr1") == 0;

              boost = boost
                || n.compare (0, 17, "boost::shared_ptr") == 0
                || n.compare (0, 19, "::boost::shared_ptr") == 0;
            }
          }
          catch (generation_failed const&)
          {
            os << fl << ":" << ln << ":" << cl << ": error: pointer_traits "
               << "specialization does not define the 'element_type' type"
               << endl;
            throw;
          }

          c = dynamic_cast<class_*> (unit.find (tn));

          if (c == 0 || !c->count ("object"))
            return 0;

          t.set ("element-type", c);

          // Determine the pointer kind.
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

            pointer_kind_type pk = static_cast<pointer_kind_type> (e);
            t.set ("pointer-kind", pk);
          }
          catch (generation_failed const&)
          {
            os << fl << ":" << ln << ":" << cl << ": error: pointer_traits "
               << "specialization does not define the 'kind' constant" << endl;
            throw;
          }

          // Get the lazy flag.
          //
          try
          {
            tree lazy (
              lookup_qualified_name (
                inst, get_identifier ("lazy"), false, false));

            if (lazy == error_mark_node || TREE_CODE (lazy) != VAR_DECL)
              throw generation_failed ();

            // Instantiate this decalaration so that we can get its value.
            //
            if (DECL_TEMPLATE_INSTANTIATION (lazy) &&
                !DECL_TEMPLATE_INSTANTIATED (lazy) &&
                !DECL_EXPLICIT_INSTANTIATION (lazy))
              instantiate_decl (lazy, false, false);

            tree init (DECL_INITIAL (lazy));

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

            t.set ("pointer-lazy", static_cast<bool> (e));
          }
          catch (generation_failed const&)
          {
            os << fl << ":" << ln << ":" << cl << ": error: pointer_traits "
               << "specialization does not define the 'kind' constant" << endl;
            throw;
          }
        }

        // Make sure the pointed-to class is complete.
        //
        if (!c->complete ())
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ": "
             << "error: pointed-to class '" << c->fq_name () << "' "
             << "is incomplete" << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: class '" << c->name () << "' is declared here" << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: consider including its definition with the "
             << "--odb-prologue option" << endl;

          throw generation_failed ();
        }

        // Make sure the pointed-to class is not abstract.
        //
        if (context::abstract (*c))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ": "
             << "error: pointed-to class '" << c->fq_name () << "' "
             << "is abstract" << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: class '" << c->name () << "' is defined here" << endl;

          throw generation_failed ();
        }

        if (m.count ("not-null") && !kp.empty ())
        {
          m.remove ("not-null");
          m.set (kp + "-not-null", string ()); // Keep compatible with pragma.
        }

        // See if this is the inverse side of a bidirectional relationship.
        // If so, then resolve the member and cache it in the context.
        //
        if (m.count ("inverse"))
        {
          string name (m.get<string> ("inverse"));
          tree decl (
            lookup_qualified_name (
              c->tree_node (), get_identifier (name.c_str ()), false, false));

          if (decl == error_mark_node || TREE_CODE (decl) != FIELD_DECL)
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ": "
               << "error: unable to resolve data member '" << name << "' "
               << "specified with '#pragma db inverse' in class '"
               << c->fq_name () << "'" << endl;
            throw generation_failed ();
          }

          data_member* im (dynamic_cast<data_member*> (unit.find (decl)));

          if (im == 0)
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ": "
               << "ice: unable to find semantic graph node corresponding to "
               << "data member '" << name << "' in class '" << c->fq_name ()
               << "'" << endl;
            throw generation_failed ();
          }

          // @@ Would be good to check that the other end is actually
          // an object pointer and is not marked as inverse. But the
          // other class may not have been processed yet.
          //
          m.remove ("inverse");
          m.set (kp + (kp.empty () ? "": "-") + "inverse", im);
        }

        return c;
      }

      tree
      instantiate_template (tree t, tree arg)
      {
        tree args (make_tree_vec (1));
        TREE_VEC_ELT (args, 0) = arg;

        // This step should succeed regardles of whether there is a
        // container traits specialization for this type.
        //
        tree inst (
          lookup_template_class (t, args, 0, 0, 0, tf_warning_or_error));

        if (inst == error_mark_node)
        {
          // Diagnostics has already been issued by lookup_template_class.
          //
          throw generation_failed ();
        }

        inst = TYPE_MAIN_VARIANT (inst);

        // The instantiation may already be complete if it matches a
        // (complete) specialization or was used before.
        //
        if (!COMPLETE_TYPE_P (inst))
          inst = instantiate_class_template (inst);

        // If we cannot instantiate this type, assume there is no suitable
        // specialization for it.
        //
        if (inst == error_mark_node || !COMPLETE_TYPE_P (inst))
          return 0;

        return inst;
      }

    private:
      tree pointer_traits_;
      tree container_traits_;
    };

    struct class_: traversal::class_, context
    {
      class_ ()
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
          assign_pointer (c);
      }

      void
      assign_pointer (type& c)
      {
        try
        {
          string ptr;
          string const& name (c.fq_name ());

          tree decl (0);    // Resolved template node.
          string decl_name; // User-provided template name.

          if (c.count ("pointer"))
          {
            string const& p (c.get<string> ("pointer"));

            if (p == "*")
              ptr = name + "*";
            else if (p[p.size () - 1] == '*')
              ptr = p;
            else if (p.find ('<') != string::npos)
            {
              // Template-id.
              //
              ptr = p;
              decl_name.assign (p, 0, p.find ('<'));
            }
            else
            {
              // This is not a template-id. Resolve it and see if it is a
              // template or a type.
              //
              decl = resolve_name (p, c.scope (), true);
              int tc (TREE_CODE (decl));

              if (tc == TYPE_DECL)
              {
                ptr = p;

                // This can be a typedef'ed alias for a TR1 template-id.
                //
                if (tree ti = TYPE_TEMPLATE_INFO (TREE_TYPE (decl)))
                {
                  decl = TI_TEMPLATE (ti); // DECL_TEMPLATE

                  // Get to the most general template declaration.
                  //
                  while (DECL_TEMPLATE_INFO (decl))
                    decl = DECL_TI_TEMPLATE (decl);
                }
                else
                  decl = 0; // Not a template.
              }
              else if (tc == TEMPLATE_DECL && DECL_CLASS_TEMPLATE_P (decl))
              {
                ptr = p + "< " + name + " >";
                decl_name = p;
              }
              else
              {
                cerr << c.file () << ":" << c.line () << ":" << c.column ()
                     << ": error: name '" << p << "' specified with "
                     << "'#pragma object pointer' does not name a type "
                     << "or a template" << endl;

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
            {
              ptr = p + "< " + name + " >";
              decl_name = p;
            }
          }

          // Check if we are using TR1.
          //
          if (decl != 0 || !decl_name.empty ())
          {
            if (!unit.count ("tr1-pointer-used"))
            {
              unit.set ("tr1-pointer-used", false);
              unit.set ("boost-pointer-used", false);
            }

            bool& tr1 (unit.get<bool> ("tr1-pointer-used"));
            bool& boost (unit.get<bool> ("boost-pointer-used"));

            // First check the user-supplied name.
            //
            tr1 = tr1
              || decl_name.compare (0, 8, "std::tr1") == 0
              || decl_name.compare (0, 10, "::std::tr1") == 0;

            // If there was no match, also resolve the name since it can be
            // a using-declaration for a TR1 template.
            //
            if (!tr1)
            {
              if (decl == 0)
                decl = resolve_name (decl_name, c.scope (), false);

              if (TREE_CODE (decl) != TEMPLATE_DECL || !
                  DECL_CLASS_TEMPLATE_P (decl))
              {
                cerr << c.file () << ":" << c.line () << ":" << c.column ()
                     << ": error: name '" << decl_name << "' specified with "
                     << "'#pragma object pointer' does not name a class "
                     << "template" << endl;

                throw generation_failed ();
              }

              string n (decl_as_string (decl, TFF_PLAIN_IDENTIFIER));

              // In case of a boost TR1 implementation, we cannot distinguish
              // between the boost:: and std::tr1:: usage since the latter is
              // just a using-declaration for the former.
              //
              tr1 = tr1
                || n.compare (0, 8, "std::tr1") == 0
                || n.compare (0, 10, "::std::tr1") == 0;

              boost = boost
                || n.compare (0, 17, "boost::shared_ptr") == 0
                || n.compare (0, 19, "::boost::shared_ptr") == 0;
            }
          }

          // Fully-qualify all the unqualified components of the name.
          //
          try
          {
            lexer.start (ptr);
            ptr.clear ();

            string t;
            bool punc (false);
            bool scoped (false);

            for (cpp_ttype tt = lexer.next (t);
                 tt != CPP_EOF;
                 tt = lexer.next (t))
            {
              if (punc && tt > CPP_LAST_PUNCTUATOR)
                ptr += ' ';

              punc = false;

              switch (static_cast<unsigned> (tt))
              {
              case CPP_LESS:
                {
                  ptr += "< ";
                  break;
                }
              case CPP_GREATER:
                {
                  ptr += " >";
                  break;
                }
              case CPP_COMMA:
                {
                  ptr += ", ";
                  break;
                }
              case CPP_NAME:
                {
                  // If the name was not preceeded with '::', look it
                  // up in the pragmas's scope and add the qualifer.
                  //
                  if (!scoped)
                  {
                    tree decl (resolve_name (t, c.scope (), false));
                    tree scope (CP_DECL_CONTEXT (decl));

                    if (scope != global_namespace)
                    {
                      ptr += "::";
                      ptr += decl_as_string (scope, TFF_PLAIN_IDENTIFIER);
                    }

                    ptr += "::";
                  }

                  ptr += t;
                  punc = true;
                  break;
                }
              case CPP_KEYWORD:
              case CPP_NUMBER:
                {
                  ptr += t;
                  punc = true;
                  break;
                }
              default:
                {
                  ptr += t;
                  break;
                }
              }

              scoped = (tt == CPP_SCOPE);
            }
          }
          catch (cxx_lexer::invalid_input const&)
          {
            throw generation_failed ();
          }

          c.set ("object-pointer", ptr);
        }
        catch (invalid_name const& ex)
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << ": error: name '" << ex.name () << "' specified with "
               << "'#pragma object pointer' is invalid" << endl;

          throw generation_failed ();
        }
        catch (unable_to_resolve const& ex)
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column ()
               << ": error: unable to resolve name '" << ex.name ()
               << "' specified with '#pragma object pointer'" << endl;

          throw generation_failed ();
        }
      }

    private:
      struct invalid_name
      {
        invalid_name (string const& n): name_ (n) {}

        string const&
        name () const {return name_;}

      private:
        string name_;
      };

      struct unable_to_resolve
      {
        unable_to_resolve (string const& n): name_ (n) {}

        string const&
        name () const {return name_;}

      private:
        string name_;
      };

      tree
      resolve_name (string const& qn, semantics::scope& ss, bool type)
      {
        tree scope (ss.tree_node ());

        // @@ Could use cxx_lexer to parse the name.
        //
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
              throw invalid_name (qn);
          }
          else
          {
            tree nid (get_identifier (n.c_str ()));
            scope = lookup_qualified_name (scope, nid, last && type, false);

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
                  s->tree_node (), nid, last && type, false);
              } while (scope == error_mark_node && !s->global_scope ());
            }

            if (scope == error_mark_node)
              throw unable_to_resolve (qn);

            if (!last && TREE_CODE (scope) == TYPE_DECL)
              scope = TREE_TYPE (scope);
          }

          if (e == string::npos)
            break;

          if (qn[++e] != ':')
            throw invalid_name (qn);

          ++e; // Second ':'.

          if (e == size)
            break;

          b = e;
        }

        return scope;
      }

    private:
      cxx_lexer lexer;

      data_member member_;
      traversal::names member_names_;
    };
  }

  void
  process_types ()
  {
    context ctx;

    traversal::unit unit;
    traversal::defines unit_defines;
    traversal::namespace_ ns;
    class_ c;

    unit >> unit_defines >> ns;
    unit_defines >> c;

    traversal::defines ns_defines;

    ns >> ns_defines >> ns;
    ns_defines >> c;

    unit.dispatch (ctx.unit);
  }
}
