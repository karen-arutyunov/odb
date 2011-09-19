// file      : odb/relational/processor.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <vector>

#include <odb/diagnostics.hxx>
#include <odb/lookup.hxx>
#include <odb/cxx-lexer.hxx>
#include <odb/common.hxx>

#include <odb/relational/context.hxx>
#include <odb/relational/processor.hxx>

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

        // Find wrapper traits.
        //
        wrapper_traits_ = lookup_qualified_name (
          odb, get_identifier ("wrapper_traits"), true, false);

        if (wrapper_traits_ == error_mark_node ||
            !DECL_CLASS_TEMPLATE_P (wrapper_traits_))
        {
          os << unit.file () << ": error: unable to resolve wrapper_traits "
             << "in the odb namespace" << endl;

          throw generation_failed ();
        }

        // Find pointer traits.
        //
        pointer_traits_ = lookup_qualified_name (
          odb, get_identifier ("pointer_traits"), true, false);

        if (pointer_traits_ == error_mark_node ||
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
        if (transient (m))
          return;

        semantics::type& t (m.type ());

        semantics::type* wt (0);
        semantics::names* wh (0);
        if (process_wrapper (t))
        {
          wt = t.get<semantics::type*> ("wrapper-type");
          wh = t.get<semantics::names*> ("wrapper-hint");
        }

        // Nothing to do if this is a composite value type.
        //
        if (composite_wrapper (t))
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

          if (type.empty ())
            type = database_type (idt, id.belongs ().hint (), true);
        }
        else
        {
          if (type.empty () && m.count ("id") && t.count ("id-type"))
            type = t.get<string> ("id-type");

          if (type.empty () && wt != 0 && m.count ("id") &&
              wt->count ("id-type"))
            type = wt->get<string> ("id-type");

          if (type.empty () && t.count ("type"))
            type = t.get<string> ("type");

          if (type.empty () && wt != 0 && wt->count ("type"))
            type = wt->get<string> ("type");

          if (type.empty ())
            type = database_type (t, m.belongs ().hint (), m.count ("id"));

          if (type.empty () && wt != 0)
            type = database_type (*wt, wh, m.count ("id"));
        }

        if (!type.empty ())
        {
          m.set ("column-type", type);

          // Issue a warning if we are relaxing null-ness.
          //
          if (m.count ("null") && m.type ().count ("not-null"))
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ":"
               << " warning: data member declared null while its type is "
               << "declared not null" << endl;
          }

          return;
        }

        // See if this is a container type.
        //
        if (process_container (m, t) ||
            (wt != 0 && process_container (m, *wt)))
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
                               semantics::names* hint,
                               semantics::data_member& m,
                               string const& prefix,
                               bool obj_ptr)
      {
        semantics::type* wt (0);
        semantics::names* wh (0);
        if (process_wrapper (t))
        {
          wt = t.get<semantics::type*> ("wrapper-type");
          wh = t.get<semantics::names*> ("wrapper-hint");
        }

        if (composite_wrapper (t))
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

          if (type.empty ())
            type = database_type (idt, id.belongs ().hint (), true);
        }
        else
        {
          if (type.empty () && t.count ("type"))
            type = t.get<string> ("type");

          if (type.empty () && wt != 0 && wt->count ("type"))
            type = wt->get<string> ("type");

          if (type.empty ())
            type = database_type (t, hint, false);

          if (type.empty () && wt != 0)
            type = database_type (*wt, wh, false);
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
      process_container (semantics::data_member& m, semantics::type& t)
      {
        // The overall idea is as follows: try to instantiate the container
        // traits class template. If we are successeful, then this is a
        // container type and we can extract the various information from
        // the instantiation. Otherwise, this is not a container.
        //

        container_kind_type ck;
        semantics::type* vt (0);
        semantics::type* it (0);
        semantics::type* kt (0);

        semantics::names* vh (0);
        semantics::names* ih (0);
        semantics::names* kh (0);

        if (t.count ("container"))
        {
          ck = t.get<container_kind_type> ("container-kind");
          vt = t.get<semantics::type*> ("value-tree-type");
          vh = t.get<semantics::names*> ("value-tree-hint");

          if (ck == ck_ordered)
          {
            it = t.get<semantics::type*> ("index-tree-type");
            ih = t.get<semantics::names*> ("index-tree-hint");
          }

          if (ck == ck_map || ck == ck_multimap)
          {
            kt = t.get<semantics::type*> ("key-tree-type");
            kh = t.get<semantics::names*> ("key-tree-hint");
          }
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

          // Mark id column as not null.
          //
          t.set ("id-not-null", true);

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

            // Find the hint.
            //
            for (tree ot (DECL_ORIGINAL_TYPE (decl));
                 ot != 0;
                 ot = decl ? DECL_ORIGINAL_TYPE (decl) : 0)
            {
              if ((vh = unit.find_hint (ot)))
                break;

              decl = TYPE_NAME (ot);
            }
          }
          catch (generation_failed const&)
          {
            os << f << ":" << l << ":" << c << ": error: "
               << "container_traits specialization does not define the "
               << "value_type type" << endl;

            throw;
          }

          t.set ("value-tree-type", vt);
          t.set ("value-tree-hint", vh);

          // If we have a set container, automatically mark the value
          // column as not null. If we already have an explicit null for
          // this column, issue an error.
          //
          if (ck == ck_set)
          {
            if (t.count ("value-null"))
            {
              os << t.file () << ":" << t.line () << ":" << t.column () << ":"
                 << " error: set container cannot contain null values" << endl;

              throw generation_failed ();
            }
            else
              t.set ("value-not-null", true);
          }

          // Issue a warning if we are relaxing null-ness in the
          // container type.
          //
          if (t.count ("value-null") && vt->count ("not-null"))
          {
            os << t.file () << ":" << t.line () << ":" << t.column () << ":"
               << " warning: container value declared null while its type "
               << "is declared not null" << endl;
          }

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

              // Find the hint.
              //
              for (tree ot (DECL_ORIGINAL_TYPE (decl));
                   ot != 0;
                   ot = decl ? DECL_ORIGINAL_TYPE (decl) : 0)
              {
                if ((ih = unit.find_hint (ot)))
                  break;

                decl = TYPE_NAME (ot);
              }
            }
            catch (generation_failed const&)
            {
              os << f << ":" << l << ":" << c << ": error: "
                 << "container_traits specialization does not define the "
                 << "index_type type" << endl;

              throw;
            }

            t.set ("index-tree-type", it);
            t.set ("index-tree-hint", ih);
            t.set ("index-not-null", true);
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

              // Find the hint.
              //
              for (tree ot (DECL_ORIGINAL_TYPE (decl));
                   ot != 0;
                   ot = decl ? DECL_ORIGINAL_TYPE (decl) : 0)
              {
                if ((kh = unit.find_hint (ot)))
                  break;

                decl = TYPE_NAME (ot);
              }
            }
            catch (generation_failed const&)
            {
              os << f << ":" << l << ":" << c << ": error: "
                 << "container_traits specialization does not define the "
                 << "key_type type" << endl;

              throw;
            }

            t.set ("key-tree-type", kt);
            t.set ("key-tree-hint", kh);
            t.set ("key-not-null", true);
          }
        }

        // Process member data.
        //
        m.set ("id-tree-type", &id_tree_type);
        m.set ("id-column-type", &id_column_type);

        process_container_value (*vt, vh, m, "value", true);

        if (it != 0)
          process_container_value (*it, ih, m, "index", false);

        if (kt != 0)
          process_container_value (*kt, kh, m, "key", false);

        // If this is an inverse side of a bidirectional object relationship
        // and it is an ordered container, mark it as unordred since there is
        // no concept of order in this construct.
        //
        if (ck == ck_ordered && m.count ("value-inverse"))
          m.set ("unordered", true);

        // Issue an error if we have a null column in a set container.
        // This can only happen if the value is declared as null in
        // the member.
        //
        if (ck == ck_set && m.count ("value-null"))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: set container cannot contain null values" << endl;

          throw generation_failed ();
        }

        // Issue a warning if we are relaxing null-ness in the member.
        //
        if (m.count ("value-null") &&
            (t.count ("value-not-null") || vt->count ("not-null")))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " warning: container value declared null while the container "
             << "type or value type declares it as not null" << endl;
        }

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

          if (c == 0 || !object (*c))
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

      bool
      process_wrapper (semantics::type& t)
      {
        if (t.count ("wrapper"))
          return t.get<bool> ("wrapper");

        // Check this type with wrapper_traits.
        //
        tree inst (instantiate_template (wrapper_traits_, t.tree_node ()));

        if (inst == 0)
        {
          t.set ("wrapper", false);
          return false;
        }

        // @@ This points to the primary template, not the specialization.
        //
        tree decl (TYPE_NAME (inst));

        string f (DECL_SOURCE_FILE (decl));
        size_t l (DECL_SOURCE_LINE (decl));
        size_t c (DECL_SOURCE_COLUMN (decl));

        // Get the wrapped type.
        //
        try
        {
          tree decl (
            lookup_qualified_name (
              inst, get_identifier ("wrapped_type"), true, false));

          if (decl == error_mark_node || TREE_CODE (decl) != TYPE_DECL)
            throw generation_failed ();

          tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));
          semantics::type& wt (
            dynamic_cast<semantics::type&> (*unit.find (type)));

          // Find the hint.
          //
          semantics::names* wh (0);

          for (tree ot (DECL_ORIGINAL_TYPE (decl));
               ot != 0;
               ot = decl ? DECL_ORIGINAL_TYPE (decl) : 0)
          {
            if ((wh = unit.find_hint (ot)))
              break;

            decl = TYPE_NAME (ot);
          }

          t.set ("wrapper-type", &wt);
          t.set ("wrapper-hint", wh);
        }
        catch (generation_failed const&)
        {
          os << f << ":" << l << ":" << c << ": error: "
             << "wrapper_traits specialization does not define the "
             << "wrapped_type type" << endl;

          throw;
        }

        // Get the null_handler flag.
        //
        bool null_handler (false);

        try
        {
          tree nh (
            lookup_qualified_name (
              inst, get_identifier ("null_handler"), false, false));

          if (nh == error_mark_node || TREE_CODE (nh) != VAR_DECL)
            throw generation_failed ();

          // Instantiate this decalaration so that we can get its value.
          //
          if (DECL_TEMPLATE_INSTANTIATION (nh) &&
              !DECL_TEMPLATE_INSTANTIATED (nh) &&
              !DECL_EXPLICIT_INSTANTIATION (nh))
            instantiate_decl (nh, false, false);

          tree init (DECL_INITIAL (nh));

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

          null_handler = static_cast<bool> (e);
          t.set ("wrapper-null-handler", null_handler);
        }
        catch (generation_failed const&)
        {
          os << f << ":" << l << ":" << c << ": error: "
             << "wrapper_traits specialization does not define the "
             << "null_handler constant" << endl;

          throw;
        }

        // Get the null_default flag.
        //
        if (null_handler)
        {
          try
          {
            tree nh (
              lookup_qualified_name (
                inst, get_identifier ("null_default"), false, false));

            if (nh == error_mark_node || TREE_CODE (nh) != VAR_DECL)
              throw generation_failed ();

            // Instantiate this decalaration so that we can get its value.
            //
            if (DECL_TEMPLATE_INSTANTIATION (nh) &&
                !DECL_TEMPLATE_INSTANTIATED (nh) &&
                !DECL_EXPLICIT_INSTANTIATION (nh))
              instantiate_decl (nh, false, false);

            tree init (DECL_INITIAL (nh));

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

            t.set ("wrapper-null-default", static_cast<bool> (e));
          }
          catch (generation_failed const&)
          {
            os << f << ":" << l << ":" << c << ": error: "
               << "wrapper_traits specialization does not define the "
               << "null_default constant" << endl;

            throw;
          }
        }

        t.set ("wrapper", true);
        return true;
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
      tree wrapper_traits_;
      tree pointer_traits_;
      tree container_traits_;
    };

    //
    //
    struct view_data_member: traversal::data_member, context
    {
      view_data_member (semantics::class_& c)
          : view_ (c),
            query_ (c.get<view_query> ("query")),
            amap_ (c.get<view_alias_map> ("alias-map")),
            omap_ (c.get<view_object_map> ("object-map"))
      {
      }

      struct assoc_member
      {
        semantics::data_member* m;
        view_object* vo;
      };

      typedef vector<assoc_member> assoc_members;

      virtual void
      traverse (semantics::data_member& m)
      {
        using semantics::data_member;

        if (transient (m))
          return;

        data_member* src_m (0); // Source member.

        // Resolve member references in column expressions.
        //
        if (m.count ("column"))
        {
          // Column literal.
          //
          if (query_.kind != view_query::condition)
          {
            warn (m.get<location_t> ("column-location"))
              << "db pragma column ignored in a view with "
              << (query_.kind == view_query::runtime ? "runtime" : "complete")
              << " query" << endl;
          }

          return;
        }
        else if (m.count ("column-expr"))
        {
          column_expr& e (m.get<column_expr> ("column-expr"));

          if (query_.kind != view_query::condition)
          {
            warn (e.loc)
              << "db pragma column ignored in a view with "
              << (query_.kind == view_query::runtime ? "runtime" : "complete")
              << " query" << endl;
            return;
          }

          for (column_expr::iterator i (e.begin ()); i != e.end (); ++i)
          {
            // This code is quite similar to translate_expression in the
            // source generator.
            //
            try
            {
              if (i->kind != column_expr_part::reference)
                continue;

              lex_.start (i->value);

              string t;
              cpp_ttype tt (lex_.next (t));

              string name;
              tree decl (0);
              semantics::class_* obj (0);

              // Check if this is an alias.
              //
              if (tt == CPP_NAME)
              {
                view_alias_map::iterator j (amap_.find (t));

                if (j != amap_.end ())
                {
                  i->table = j->first;
                  obj = j->second->object;

                  // Skip '::'.
                  //
                  if (lex_.next (t) != CPP_SCOPE)
                  {
                    error (i->loc)
                      << "member name expected after an alias in db pragma "
                      << "column" << endl;
                    throw generation_failed ();
                  }

                  tt = lex_.next (t);

                  cpp_ttype ptt; // Not used.
                  decl = lookup::resolve_scoped_name (
                    t, tt, ptt, lex_, obj->tree_node (), name, false);
                }
              }

              // If it is not an alias, do the normal lookup.
              //
              if (obj == 0)
              {
                // Also get the object type. We need to do it so that
                // we can get the correct (derived) table name (the
                // member can come from a base class).
                //
                tree type;
                cpp_ttype ptt; // Not used.
                decl = lookup::resolve_scoped_name (
                  t, tt, ptt, lex_, i->scope, name, false, &type);

                type = TYPE_MAIN_VARIANT (type);

                view_object_map::iterator j (omap_.find (type));

                if (j == omap_.end ())
                {
                  error (i->loc)
                    << "name '" << name << "' in db pragma column does not "
                    << "refer to a data member of a persistent class that "
                    << "is used in this view" << endl;
                  throw generation_failed ();
                }

                obj = j->second->object;
                i->table = table_name (*obj);
              }

              // Check that we have a data member.
              //
              if (TREE_CODE (decl) != FIELD_DECL)
              {
                error (i->loc) << "name '" << name << "' in db pragma column "
                               << "does not refer to a data member" << endl;
                throw generation_failed ();
              }

              data_member* m (dynamic_cast<data_member*> (unit.find (decl)));
              i->member_path.push_back (m);

              // Finally, resolve nested members if any.
              //
              for (; tt == CPP_DOT; tt = lex_.next (t))
              {
                lex_.next (t); // Get CPP_NAME.

                tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

                decl = lookup_qualified_name (
                  type, get_identifier (t.c_str ()), false, false);

                if (decl == error_mark_node || TREE_CODE (decl) != FIELD_DECL)
                {
                  error (i->loc) << "name '" << t << "' in db pragma column "
                                 << "does not refer to a data member" << endl;
                  throw generation_failed ();
                }

                m = dynamic_cast<data_member*> (unit.find (decl));
                i->member_path.push_back (m);
              }

              // If the expression is just this reference, then we have
              // a source member.
              //
              if (e.size () == 1)
                src_m = m;
            }
            catch (lookup::invalid_name const&)
            {
              error (i->loc) << "invalid name in db pragma column" << endl;
              throw generation_failed ();
            }
            catch (lookup::unable_to_resolve const& e)
            {
              error (i->loc) << "unable to resolve name '" << e.name ()
                             << "' in db pragma column" << endl;
              throw generation_failed ();
            }
          }

          // We have the source member, check that the C++ types are the
          // same (sans cvr-qualification and wrapping) and issue a warning
          // if they differ. In rare cases where this is not a mistake, the
          // user can a phony expression (e.g., "" + person:name) to disable
          // the warning. Note that in this case there will be no type pragma
          // copying, which is probably ok seeing that the C++ types are
          // different.
          //
          //
          if (src_m != 0 &&
              !member_resolver::check_types (m.type (), src_m->type ()))
          {
            warn (e.loc)
              << "object data member '" << src_m->name () << "' specified "
              << "in db pragma column has a different type compared to the "
              << "view data member" << endl;

            info (src_m->file (), src_m->line (), src_m->column ())
              << "object data member is defined here" << endl;

            info (m.file (), m.line (), m.column ())
              << "view data member is defined here" << endl;
          }
        }
        // This member has no column information. If we are generting our
        // own query, try to find a member with the same (or similar) name
        // in one of the associated objects.
        //
        else if (query_.kind == view_query::condition)
        {
          view_objects& objs (view_.get<view_objects> ("objects"));

          assoc_members exact_members, pub_members;
          member_resolver resolver (exact_members, pub_members, m);

          for (view_objects::iterator i (objs.begin ()); i != objs.end (); ++i)
            resolver.traverse (*i);

          assoc_members& members (
            !exact_members.empty () ? exact_members : pub_members);

          // Issue diagnostics if we didn't find any or found more
          // than one.
          //
          if (members.empty ())
          {
            error (m.file (), m.line (), m.column ())
              << "unable to find a corresponding data member for '"
              << m.name () << "' in any of the associated objects" << endl;

            info (m.file (), m.line (), m.column ())
              << "use db pragma column to specify the corresponding data "
              << "member or column name" << endl;

            throw generation_failed ();
          }
          else if (members.size () > 1)
          {
            error (m.file (), m.line (), m.column ())
              << "corresponding data member for '" << m.name () << "' is "
              << "ambiguous" << endl;

            info (m.file (), m.line (), m.column ())
              << "candidates are:" << endl;

            for (assoc_members::const_iterator i (members.begin ());
                 i != members.end ();
                 ++i)
            {
              info (i->m->file (), i->m->line (), i->m->column ())
                << "  '" << i->m->name () << "' in object '"
                << i->vo->name () << "'" << endl;
            }

            info (m.file (), m.line (), m.column ())
              << "use db pragma column to resolve this ambiguity" << endl;

            throw generation_failed ();
          }

          // Synthesize the column expression for this member.
          //
          assoc_member const& am (members.back ());

          column_expr& e (m.set ("column-expr", column_expr ()));
          e.push_back (column_expr_part ());
          column_expr_part& ep (e.back ());

          ep.kind = column_expr_part::reference;
          ep.table = am.vo->alias.empty ()
            ? table_name (*am.vo->object)
            : am.vo->alias;
          ep.member_path.push_back (am.m);

          src_m = am.m;
        }

        // If we have the source member and don't have the type pragma of
        // our own, but the source member does, then copy the columnt type
        // over.
        //
        if (src_m != 0 && !m.count ("type") && src_m->count ("type"))
          m.set ("column-type", src_m->get<string> ("column-type"));

        // Check the return statements above if you add any extra logic
        // here.
      }

      struct member_resolver: traversal::class_
      {
        member_resolver (assoc_members& members,
                         assoc_members& pub_members,
                         semantics::data_member& m)
            : member_ (members, pub_members, m)
        {
          *this >> names_ >> member_;
          *this >> inherits_ >> *this;
        }

        void
        traverse (view_object& vo)
        {
          member_.vo_ = &vo;
          traverse (*vo.object);
        }

        virtual void
        traverse (type& c)
        {
          if (!object (c))
            return; // Ignore transient bases.

          names (c);
          inherits (c);
        }

      public:
        static bool
        check_types (semantics::type& t1, semantics::type& t2)
        {
          using semantics::type;
          using semantics::derived_type;

          // Require that the types be the same sans the wrapping and
          // cvr-qualification.
          //
          type* pt1 (&t1);
          type* pt2 (&t2);

          if (type* wt1 = context::wrapper (*pt1))
            pt1 = wt1;

          if (type* wt2 = context::wrapper (*pt2))
            pt2 = wt2;

          if (derived_type* dt1 = dynamic_cast<derived_type*> (pt1))
            pt1 = &dt1->base_type ();

          if (derived_type* dt2 = dynamic_cast<derived_type*> (pt2))
            pt2 = &dt2->base_type ();

          if (pt1 != pt2)
            return false;

          return true;
        }

      private:
        struct data_member: traversal::data_member
        {
          data_member (assoc_members& members,
                       assoc_members& pub_members,
                       semantics::data_member& m)
              : members_ (members),
                pub_members_ (pub_members),
                name_ (m.name ()),
                pub_name_ (context::current ().public_name (m)),
                type_ (m.type ())
          {
          }

          virtual void
          traverse (type& m)
          {
            // First see if we have the exact match.
            //
            if (name_ == m.name ())
            {
              if (check (m))
              {
                assoc_member am;
                am.m = &m;
                am.vo = vo_;
                members_.push_back (am);
              }

              return;
            }

            // Don't bother with public name matching if we already
            // have an exact match.
            //
            if (members_.empty ())
            {
              if (pub_name_ == context::current ().public_name (m))
              {
                if (check (m))
                {
                  assoc_member am;
                  am.m = &m;
                  am.vo = vo_;
                  pub_members_.push_back (am);
                }

                return;
              }
            }
          }

          bool
          check (semantics::data_member& m)
          {
            // Make sure that the found node can possibly match.
            //
            if (context::transient (m) || context::inverse (m))
              return false;

            return check_types (m.type (), type_);
          }

          assoc_members& members_;
          assoc_members& pub_members_;

          string name_;
          string pub_name_;
          semantics::type& type_;

          view_object* vo_;
        };

        traversal::names names_;
        data_member member_;
        traversal::inherits inherits_;
      };

    private:
      semantics::class_& view_;
      view_query& query_;
      view_alias_map& amap_;
      view_object_map& omap_;
      cxx_string_lexer lex_;
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
        class_kind_type k (class_kind (c));

        if (k == class_other)
          return;

        names (c);

        // Assign pointer.
        //
        if (k == class_object || k == class_view)
          assign_pointer (c);

        // Do some additional processing for views.
        //
        if (k == class_view)
          traverse_view (c);
      }

      //
      // View.
      //

      struct relationship
      {
        semantics::data_member* member;
        string name;
        view_object* pointer;
        view_object* pointee;
      };

      typedef vector<relationship> relationships;

      virtual void
      traverse_view (type& c)
      {
        bool has_q (c.count ("query"));
        bool has_o (c.count ("objects"));

        // Determine the kind of query template we've got.
        //
        view_query& vq (has_q
                        ? c.get<view_query> ("query")
                        : c.set ("query", view_query ()));
        if (has_q)
        {
          if (!vq.literal.empty ())
          {
            string q (upcase (vq.literal));
            vq.kind = (q.compare (0, 7, "SELECT ") == 0)
              ? view_query::complete
              : view_query::condition;
          }
          else if (!vq.expr.empty ())
          {
            // If the first token in the expression is a string and
            // it starts with "SELECT " or is equal to "SELECT", then
            // we have a complete query.
            //
            if (vq.expr.front ().type == CPP_STRING)
            {
              string q (upcase (vq.expr.front ().literal));
              vq.kind = (q.compare (0, 7, "SELECT ") == 0 || q == "SELECT")
                ? view_query::complete
                : view_query::condition;
            }
            else
              vq.kind = view_query::condition;
          }
          else
            vq.kind = view_query::runtime;
        }
        else
          vq.kind = has_o ? view_query::condition : view_query::runtime;

        // We cannot have an incomplete query if there are not objects
        // to derive the rest from.
        //
        if (vq.kind == view_query::condition && !has_o)
        {
          error (c.file (), c.line (), c.column ())
            << "view '" << c.fq_name () << "' has an incomplete query "
            << "template and no associated objects" << endl;

          info (c.file (), c.line (), c.column ())
            << "use db pragma query to provide a complete query template"
            << endl;

          info (c.file (), c.line (), c.column ())
            << "or use db pragma object to associate one or more objects "
            << "with the view"
            << endl;

          throw generation_failed ();
        }

        // Resolve referenced objects from tree nodes to semantic graph
        // nodes.
        //
        view_alias_map& amap (c.set ("alias-map", view_alias_map ()));
        view_object_map& omap (c.set ("object-map", view_object_map ()));

        if (has_o)
        {
          using semantics::class_;

          view_objects& objs (c.get<view_objects> ("objects"));

          for (view_objects::iterator i (objs.begin ()); i != objs.end (); ++i)
          {
            tree n (TYPE_MAIN_VARIANT (i->node));

            if (TREE_CODE (n) != RECORD_TYPE)
            {
              error (i->loc)
                << "name '" << i->orig_name << "' in db pragma object does "
                << "not name a class" << endl;

              throw generation_failed ();
            }

            class_& o (dynamic_cast<class_&> (*unit.find (n)));

            if (!object (o))
            {
              error (i->loc)
                << "name '" << i->orig_name << "' in db pragma object does "
                << "not name a persistent class" << endl;

              info (o.file (), o.line (), o.column ())
                << "class '" << i->orig_name << "' is defined here" << endl;

              throw generation_failed ();
            }

            i->object = &o;

            if (i->alias.empty ())
            {
              if (!omap.insert (view_object_map::value_type (n, &*i)).second)
              {
                error (i->loc)
                  << "persistent class '" << i->orig_name << "' is used in "
                  << "the view more than once" << endl;

                info (i->loc)
                  << "use the alias clause to assign it a different name"
                  << endl;

                throw generation_failed ();
              }
            }
            else
            {
              if (!amap.insert (
                    view_alias_map::value_type (i->alias, &*i)).second)
              {
                error (i->loc)
                  << "alias '" << i->alias << "' is used in the view more "
                  << "than once" << endl;

                throw generation_failed ();
              }
            }

            // If we have to generate the query and there was no JOIN
            // condition specified by the user, try to come up with one
            // automatically based on object relationships.
            //
            if (vq.kind == view_query::condition &&
                i->cond.empty () &&
                i != objs.begin ())
            {
              relationships rs;

              // Check objects specified prior to this one for any
              // relationships. We don't examine objects that were
              // specified after this one because that would require
              // rearranging the JOIN order.
              //
              for (view_objects::iterator j (objs.begin ()); j != i; ++j)
              {
                // First see if any of the objects that were specified
                // prior to this object point to it.
                //
                {
                  relationship_resolver r (rs, *i, true);
                  r.traverse (*j);
                }

                // Now see if this object points to any of the objects
                // specified prior to it. Ignore self-references if any,
                // since they were already added to the list in the
                // previous pass.
                //
                {
                  relationship_resolver r (rs, *j, false);
                  r.traverse (*i);
                }
              }

              // Issue diagnostics if we didn't find any or found more
              // than one.
              //
              if (rs.empty ())
              {
                error (i->loc)
                  << "unable to find an object relationship involving "
                  << "object '" << i->name () << "' and any of the previously "
                  << "associated objects" << endl;

                info (i->loc)
                  << "use the join condition clause in db pragma object "
                  << "to specify a custom join condition" << endl;

                throw generation_failed ();
              }
              else if (rs.size () > 1)
              {
                error (i->loc)
                  << "object relationship for object '" << i->name () <<  "' "
                  << "is ambiguous" << endl;

                info (i->loc)
                  << "candidates are:" << endl;

                for (relationships::const_iterator j (rs.begin ());
                     j != rs.end ();
                     ++j)
                {
                  semantics::data_member& m (*j->member);

                  info (m.file (), m.line (), m.column ())
                    << "  '" << j->name << "' "
                    << "in object '" << j->pointer->name () << "' "
                    << "pointing to '" << j->pointee->name () << "'"
                    << endl;
                }

                info (i->loc)
                  << "use the join condition clause in db pragma object "
                  << "to resolve this ambiguity" << endl;

                throw generation_failed ();
              }

              // Synthesize the condition.
              //
              relationship const& r (rs.back ());

              string name (r.pointer->alias.empty ()
                           ? r.pointer->object->fq_name ()
                           : r.pointer->alias);
              name += "::";
              name += r.name;

              lexer.start (name);

              string t;
              for (cpp_ttype tt (lexer.next (t));
                   tt != CPP_EOF;
                   tt = lexer.next (t))
              {
                cxx_token ct;
                ct.type = tt;
                ct.literal = t;
                i->cond.push_back (ct);
              }
            }
          }
        }

        // Handle data members.
        //
        {
          view_data_member t (c);
          traversal::names n (t);
          names (c, n);
        }
      }

      struct relationship_resolver: object_members_base
      {
        relationship_resolver (relationships& rs,
                               view_object& pointee,
                               bool self_pointer)
            : object_members_base (false, false, true),
              relationships_ (rs),
              self_pointer_ (self_pointer),
              pointer_ (0),
              pointee_ (pointee)
        {
        }

        void
        traverse (view_object& pointer)
        {
          pointer_ = &pointer;
          object_members_base::traverse (*pointer.object);
        }

        virtual void
        traverse_simple (semantics::data_member& m)
        {
          if (semantics::class_* c = object_pointer (m.type ()))
          {
            // Ignore inverse sides of the same relationship to avoid
            // phony conflicts caused by the direct side that will end
            // up in the relationship list as well.
            //
            if (inverse (m))
              return;

            // Ignore self-pointers if requested.
            //
            if (!self_pointer_ && pointer_->object == c)
              return;

            if (pointee_.object == c)
            {
              relationships_.push_back (relationship ());
              relationships_.back ().member = &m;
              relationships_.back ().name = member_prefix_ + m.name ();
              relationships_.back ().pointer = pointer_;
              relationships_.back ().pointee = &pointee_;
            }
          }
        }

        virtual void
        traverse_container (semantics::data_member& m, semantics::type& t)
        {
          if (semantics::class_* c =
              object_pointer (context::container_vt (t)))
          {
            if (inverse (m, "value"))
              return;

            // Ignore self-pointers if requested.
            //
            if (!self_pointer_ && pointer_->object == c)
              return;

            if (pointee_.object == c)
            {
              relationships_.push_back (relationship ());
              relationships_.back ().member = &m;
              relationships_.back ().name = member_prefix_ + m.name ();
              relationships_.back ().pointer = pointer_;
              relationships_.back ().pointee = &pointee_;
            }
          }
        }

      private:
        relationships& relationships_;
        bool self_pointer_;
        view_object* pointer_;
        view_object& pointee_;
      };

      void
      assign_pointer (type& c)
      {
        location_t loc (0);     // Pragma location, or 0 if not used.

        try
        {
          string ptr;
          string const& name (c.fq_name ());

          tree decl (0);          // Resolved template node.
          string decl_name;       // User-provided template name.
          tree resolve_scope (0); // Scope in which we resolve names.

          if (c.count ("pointer"))
          {
            class_pointer const& cp (c.get<class_pointer> ("pointer"));
            string const& p (cp.name);

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
              decl = resolve_name (p, cp.scope, true);
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
                error (cp.loc)
                  << "name '" << p << "' specified with db pragma pointer "
                  << "does not name a type or a template" << endl;

                throw generation_failed ();
              }
            }

            // Resolve scope is the scope of the pragma.
            //
            resolve_scope = cp.scope;
            loc = cp.loc;
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

            // Resolve scope is the scope of the class.
            //
            resolve_scope = c.scope ().tree_node ();
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
                decl = resolve_name (decl_name, resolve_scope, false);

              if (TREE_CODE (decl) != TEMPLATE_DECL || !
                  DECL_CLASS_TEMPLATE_P (decl))
              {
                // This is only checked for the --default-pointer option.
                //
                error (c.file (), c.line (), c.column ())
                  << "name '" << decl_name << "' specified with the "
                  << "--default-pointer option does not name a class "
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

            for (cpp_ttype tt (lexer.next (t));
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
                    tree decl (resolve_name (t, resolve_scope, false));
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
          if (loc != 0)
            error (loc)
              << "name '" << ex.name () << "' specified with db pragma "
              << "pointer is invalid" << endl;
          else
            error (c.file (), c.line (), c.column ())
              << "name '" << ex.name () << "' specified with the "
              << "--default-pointer option is invalid" << endl;


          throw generation_failed ();
        }
        catch (unable_to_resolve const& ex)
        {
          if (loc != 0)
            error (loc)
              << "unable to resolve name '" << ex.name () << "' specified "
              << "with db pragma pointer" << endl;
          else
            error (c.file (), c.line (), c.column ())
              << "unable to resolve name '" << ex.name () << "' specified "
              << "with the --default-pointer option" << endl;

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

      typedef lookup::unable_to_resolve unable_to_resolve;

      tree
      resolve_name (string const& qn, tree scope, bool is_type)
      {
        try
        {
          string t;
          cpp_ttype tt, ptt;

          nested_lexer.start (qn);
          tt = nested_lexer.next (t);

          string name;
          return lookup::resolve_scoped_name (
            t, tt, ptt, nested_lexer, scope, name, is_type);
        }
        catch (cxx_lexer::invalid_input const&)
        {
          throw invalid_name (qn);
        }
        catch (lookup::invalid_name const&)
        {
          throw invalid_name (qn);
        }
      }

    private:
      cxx_string_lexer lexer;
      cxx_string_lexer nested_lexer;

      data_member member_;
      traversal::names member_names_;
    };
  }

  void
  process ()
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
