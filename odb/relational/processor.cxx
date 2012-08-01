// file      : odb/relational/processor.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <vector>
#include <algorithm>

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

          throw operation_failed ();
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

          throw operation_failed ();
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

          throw operation_failed ();
        }

        // Find the access class.
        //
        tree access = lookup_qualified_name (
          odb, get_identifier ("access"), true, false);

        if (access == error_mark_node)
        {
          os << unit.file () << ": error: unable to resolve access class"
             << "in the odb namespace" << endl;

          throw operation_failed ();
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

          throw operation_failed ();
        }
      }

      virtual void
      traverse (semantics::data_member& m)
      {
        if (transient (m))
          return;

        process_index (m);

        semantics::names* hint;
        semantics::type& t (utype (m, hint));

        // Handle wrappers.
        //
        semantics::type* wt (0), *qwt (0);
        semantics::names* whint (0);
        if (process_wrapper (t))
        {
          qwt = t.get<semantics::type*> ("wrapper-type");
          whint = t.get<semantics::names*> ("wrapper-hint");
          wt = &utype (*qwt, whint);
        }

        // If the type is const and the member is not id, version, or
        // inverse, then mark it as readonly. In case of a wrapper,
        // both the wrapper type and the wrapped type must be const.
        // To see why, consider these possibilities:
        //
        // auto_ptr<const T> - can modify by setting a new pointer
        // const auto_ptr<T> - can modify by changing the pointed-to value
        //
        if (const_type (m.type ()) &&
            !(id (m) || version (m) || m.count ("inverse")))
        {
          if (qwt == 0 || const_type (*qwt))
            m.set ("readonly", true);
        }

        // Nothing to do if this is a composite value type.
        //
        if (composite_wrapper (t))
          return;

        string type, id_type;

        if (m.count ("id-type"))
          id_type = m.get<string> ("id-type");

        if (m.count ("type"))
        {
          type = m.get<string> ("type");

          if (id_type.empty ())
            id_type = type;
        }

        if (semantics::class_* c = process_object_pointer (m, t))
        {
          // This is an object pointer. The column type is the pointed-to
          // object id type.
          //
          semantics::data_member& id (*id_member (*c));

          semantics::names* idhint;
          semantics::type& idt (utype (id, idhint));

          semantics::type* wt (0);
          semantics::names* whint (0);
          if (process_wrapper (idt))
          {
            whint = idt.get<semantics::names*> ("wrapper-hint");
            wt = &utype (*idt.get<semantics::type*> ("wrapper-type"), whint);
          }

          // Nothing to do if this is a composite value type.
          //
          if (composite_wrapper (idt))
            return;

          if (type.empty () && id.count ("id-type"))
            type = id.get<string> ("id-type");

          if (type.empty () && id.count ("type"))
            type = id.get<string> ("type");

          // The rest should be identical to the code for the id_type in
          // the else block.
          //
          if (type.empty () && idt.count ("id-type"))
            type = idt.get<string> ("id-type");

          if (type.empty () && wt != 0 && wt->count ("id-type"))
            type = wt->get<string> ("id-type");

          if (type.empty () && idt.count ("type"))
            type = idt.get<string> ("type");

          if (type.empty () && wt != 0 && wt->count ("type"))
            type = wt->get<string> ("type");

          if (type.empty ())
            type = database_type (idt, idhint, true);

          if (type.empty () && wt != 0)
            type = database_type (*wt, whint, true);

          id_type = type;
        }
        else
        {
          if (id_type.empty () && t.count ("id-type"))
            id_type = t.get<string> ("id-type");

          if (id_type.empty () && wt != 0 && wt->count ("id-type"))
            id_type = wt->get<string> ("id-type");

          if (type.empty () && t.count ("type"))
            type = t.get<string> ("type");

          if (type.empty () && wt != 0 && wt->count ("type"))
            type = wt->get<string> ("type");

          if (id_type.empty ())
            id_type = type;

          if (id_type.empty ())
            id_type = database_type (t, hint, true);

          if (type.empty ())
            type = database_type (t, hint, false);

          if (id_type.empty () && wt != 0)
            id_type = database_type (*wt, whint, true);

          if (type.empty () && wt != 0)
            type = database_type (*wt, whint, false);

          // Use id mapping for discriminators.
          //
          if (id (m) || discriminator (m))
            type = id_type;
        }

        if (!type.empty ())
        {
          m.set ("column-type", type);
          m.set ("column-id-type", id_type);

          // Issue a warning if we are relaxing null-ness.
          //
          if (m.count ("null") && t.count ("not-null"))
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
        os << m.file () << ":" << m.line () << ":" << m.column () << ":"
           << " error: unable to map C++ type '" << t.fq_name (hint)
           << "' used in data member '" << m.name () << "' to a "
           << "database type" << endl;

        os << m.file () << ":" << m.line () << ":" << m.column () << ":"
           << " info: use '#pragma db type' to specify the database type"
           << endl;

        throw operation_failed ();
      }

      // Convert index/unique specifiers to the index entry in the object.
      //
      void
      process_index (semantics::data_member& m)
      {
        bool ip (m.count ("index"));
        bool up (m.count ("unique"));

        if (ip || up)
        {
          using semantics::class_;
          class_& c (dynamic_cast<class_&> (m.scope ()));

          indexes& ins (c.count ("index")
                        ? c.get<indexes> ("index")
                        : c.set ("index", indexes ()));

          index in;
          in.loc = m.get<location_t> (
            ip ? "index-location" : "unique-location");

          if (up)
            in.type = "UNIQUE";

          index::member im;
          im.loc = in.loc;
          im.name = m.name ();
          im.path.push_back (&m);
          in.members.push_back (im);

          // Insert it in the location order.
          //
          ins.insert (
            lower_bound (ins.begin (), ins.end (), in, index_comparator ()),
            in);
        }
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
        semantics::type& ct (utype (m));

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
          // object id type.
          //
          semantics::data_member& id (*id_member (*c));

          semantics::names* idhint;
          semantics::type& idt (utype (id, idhint));

          semantics::type* wt (0);
          semantics::names* whint (0);
          if (process_wrapper (idt))
          {
            whint = idt.get<semantics::names*> ("wrapper-hint");
            wt = &utype (*idt.get<semantics::type*> ("wrapper-type"), whint);
          }

          // Nothing to do if this is a composite value type.
          //
          if (composite_wrapper (idt))
            return;

          if (type.empty () && id.count ("id-type"))
            type = id.get<string> ("id-type");

          if (type.empty () && id.count ("type"))
            type = id.get<string> ("type");

          // The rest of the code is identical to the else block except here
          // we have to check for "id-type" before checking for "type".
          //

          if (type.empty () && idt.count ("id-type"))
            type = idt.get<string> ("id-type");

          if (type.empty () && wt != 0 && wt->count ("id-type"))
            type = wt->get<string> ("id-type");

          if (type.empty () && idt.count ("type"))
            type = idt.get<string> ("type");

          if (type.empty () && wt != 0 && wt->count ("type"))
            type = wt->get<string> ("type");

          if (type.empty ())
            type = database_type (idt, idhint, true);

          if (type.empty () && wt != 0)
            type = database_type (*wt, whint, true);
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
          m.set (prefix + "-column-id-type", type);
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

        throw operation_failed ();
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
              throw operation_failed ();


            // Instantiate this decalaration so that we can get its value.
            //
            if (DECL_TEMPLATE_INSTANTIATION (kind) &&
                !DECL_TEMPLATE_INSTANTIATED (kind) &&
                !DECL_EXPLICIT_INSTANTIATION (kind))
              instantiate_decl (kind, false, false);

            tree init (DECL_INITIAL (kind));

            if (init == error_mark_node || TREE_CODE (init) != INTEGER_CST)
              throw operation_failed ();

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
          catch (operation_failed const&)
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
              throw operation_failed ();

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
          catch (operation_failed const&)
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

              throw operation_failed ();
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
                throw operation_failed ();

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
            catch (operation_failed const&)
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
                throw operation_failed ();

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
            catch (operation_failed const&)
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

          throw operation_failed ();
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
              throw operation_failed ();

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

              bool& tr1 (features.tr1_pointer);
              bool& boost (features.boost_pointer);

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
          catch (operation_failed const&)
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
              throw operation_failed ();

            // Instantiate this decalaration so that we can get its value.
            //
            if (DECL_TEMPLATE_INSTANTIATION (kind) &&
                !DECL_TEMPLATE_INSTANTIATED (kind) &&
                !DECL_EXPLICIT_INSTANTIATION (kind))
              instantiate_decl (kind, false, false);

            tree init (DECL_INITIAL (kind));

            if (init == error_mark_node || TREE_CODE (init) != INTEGER_CST)
              throw operation_failed ();

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
          catch (operation_failed const&)
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
              throw operation_failed ();

            // Instantiate this decalaration so that we can get its value.
            //
            if (DECL_TEMPLATE_INSTANTIATION (lazy) &&
                !DECL_TEMPLATE_INSTANTIATED (lazy) &&
                !DECL_EXPLICIT_INSTANTIATION (lazy))
              instantiate_decl (lazy, false, false);

            tree init (DECL_INITIAL (lazy));

            if (init == error_mark_node || TREE_CODE (init) != INTEGER_CST)
              throw operation_failed ();

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
          catch (operation_failed const&)
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
             << "error: pointed-to class '" << class_fq_name (*c) << "' "
             << "is incomplete" << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: class '" << class_name (*c) << "' is declared here"
             << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: consider including its definition with the "
             << "--odb-epilogue option" << endl;

          throw operation_failed ();
        }

        // Make sure the pointed-to class is not reuse-abstract.
        //
        if (abstract (*c) && !polymorphic (*c))
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ": "
             << "error: pointed-to class '" << class_fq_name (*c) << "' "
             << "is abstract" << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: class '" << class_name (*c) << "' is defined here"
             << endl;

          throw operation_failed ();
        }

        // Make sure the pointed-to class has object id.
        //
        if (id_member (*c) == 0)
        {
          os << m.file () << ":" << m.line () << ":" << m.column () << ": "
             << "error: pointed-to class '" << class_fq_name (*c) << "' "
             << "has no object id" << endl;

          os << c->file () << ":" << c->line () << ":" << c->column () << ": "
             << "info: class '" << class_name (*c) << "' is defined here"
             << endl;

          throw operation_failed ();
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
               << class_fq_name (*c) << "'" << endl;
            throw operation_failed ();
          }

          data_member* im (dynamic_cast<data_member*> (unit.find (decl)));

          if (im == 0)
          {
            os << m.file () << ":" << m.line () << ":" << m.column () << ": "
               << "ice: unable to find semantic graph node corresponding to "
               << "data member '" << name << "' in class '"
               << class_fq_name (*c) << "'" << endl;
            throw operation_failed ();
          }

          // @@ Would be good to check that the other end is actually
          // an object pointer, is not marked as inverse, and points
          // to the correct object. But the other class may not have
          // been processed yet.
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
            throw operation_failed ();

          // The wrapped_type alias is a typedef in an instantiation
          // that we just instantiated dynamically. As a result there
          // is no semantic graph edges corresponding to this typedef
          // since we haven't parsed it yet. So to get the tree node
          // that can actually be resolved to the graph node, we use
          // the source type of this typedef.
          //
          tree type (DECL_ORIGINAL_TYPE (decl));

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
        catch (operation_failed const&)
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
            throw operation_failed ();

          // Instantiate this decalaration so that we can get its value.
          //
          if (DECL_TEMPLATE_INSTANTIATION (nh) &&
              !DECL_TEMPLATE_INSTANTIATED (nh) &&
              !DECL_EXPLICIT_INSTANTIATION (nh))
            instantiate_decl (nh, false, false);

          tree init (DECL_INITIAL (nh));

          if (init == error_mark_node || TREE_CODE (init) != INTEGER_CST)
            throw operation_failed ();

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
        catch (operation_failed const&)
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
              throw operation_failed ();

            // Instantiate this decalaration so that we can get its value.
            //
            if (DECL_TEMPLATE_INSTANTIATION (nh) &&
                !DECL_TEMPLATE_INSTANTIATED (nh) &&
                !DECL_EXPLICIT_INSTANTIATION (nh))
              instantiate_decl (nh, false, false);

            tree init (DECL_INITIAL (nh));

            if (init == error_mark_node || TREE_CODE (init) != INTEGER_CST)
              throw operation_failed ();

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
          catch (operation_failed const&)
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
        // specialization for this type.
        //
        tree inst (
          lookup_template_class (t, args, 0, 0, 0, tf_warning_or_error));

        if (inst == error_mark_node)
        {
          // Diagnostics has already been issued by lookup_template_class.
          //
          throw operation_failed ();
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

              string tl;
              tree tn;
              cpp_ttype tt (lex_.next (tl, &tn));

              string name;
              tree decl (0);
              view_object* vo (0);

              // Check if this is an alias.
              //
              if (tt == CPP_NAME)
              {
                view_alias_map::iterator j (amap_.find (tl));

                if (j != amap_.end ())
                {
                  vo = j->second;

                  // Skip '::'.
                  //
                  if (lex_.next (tl, &tn) != CPP_SCOPE)
                  {
                    error (i->loc)
                      << "member name expected after an alias in db pragma "
                      << "column" << endl;
                    throw operation_failed ();
                  }

                  tt = lex_.next (tl, &tn);

                  cpp_ttype ptt; // Not used.
                  decl = lookup::resolve_scoped_name (
                    lex_, tt, tl, tn, ptt, vo->obj->tree_node (), name, false);
                }
              }

              // If it is not an alias, do the normal lookup.
              //
              if (vo == 0)
              {
                // Also get the object type. We need to do it so that
                // we can get the correct (derived) table name (the
                // member can come from a base class).
                //
                tree type;
                cpp_ttype ptt; // Not used.
                decl = lookup::resolve_scoped_name (
                  lex_, tt, tl, tn, ptt, i->scope, name, false, false, &type);

                type = TYPE_MAIN_VARIANT (type);

                view_object_map::iterator j (omap_.find (type));

                if (j == omap_.end ())
                {
                  error (i->loc)
                    << "name '" << name << "' in db pragma column does not "
                    << "refer to a data member of a persistent class that "
                    << "is used in this view" << endl;
                  throw operation_failed ();
                }

                vo = j->second;
              }

              // Check that we have a data member.
              //
              if (TREE_CODE (decl) != FIELD_DECL)
              {
                error (i->loc) << "name '" << name << "' in db pragma column "
                               << "does not refer to a data member" << endl;
                throw operation_failed ();
              }

              data_member* m (dynamic_cast<data_member*> (unit.find (decl)));
              i->member_path.push_back (m);

              // Figure out the table name/alias for this member.
              //
              using semantics::class_;

              if (class_* root = polymorphic (*vo->obj))
              {
                // If the object is polymorphic, then figure out which of the
                // bases this member comes from and use the corresponding
                // table.
                //
                class_* c (&static_cast<class_&> (m->scope ()));

                // If this member's class is not polymorphic (root uses reuse
                // inheritance), then use the root table.
                //
                if (!polymorphic (*c))
                  c = root;

                // In a polymorphic hierarchy we have several tables and the
                // provided alias is used as a prefix together with the table
                // name to form the actual alias.
                //
                qname const& t (table_name (*c));

                if (vo->alias.empty ())
                  i->table = t;
                else
                  i->table = qname (vo->alias + "_" + t.uname ());
              }
              else
                i->table = vo->alias.empty ()
                  ? table_name (*vo->obj)
                  : qname (vo->alias);

              // Finally, resolve nested members if any.
              //
              for (; tt == CPP_DOT; tt = lex_.next (tl, &tn))
              {
                lex_.next (tl, &tn); // Get CPP_NAME.

                tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

                decl = lookup_qualified_name (
                  type, get_identifier (tl.c_str ()), false, false);

                if (decl == error_mark_node || TREE_CODE (decl) != FIELD_DECL)
                {
                  error (i->loc) << "name '" << tl << "' in db pragma column "
                                 << "does not refer to a data member" << endl;
                  throw operation_failed ();
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
              throw operation_failed ();
            }
            catch (lookup::unable_to_resolve const& e)
            {
              error (i->loc) << "unable to resolve name '" << e.name ()
                             << "' in db pragma column" << endl;
              throw operation_failed ();
            }
          }

          // Check that the source member is not transient or inverse. Also
          // check that the C++ types are the same (sans cvr-qualification
          // and wrapping) and issue a warning if they differ. In rare cases
          // where this is not a mistake, the user can use a phony expression
          // (e.g., "" + person:name) to disable the warning. Note that in
          // this case there will be no type pragma copying, which is probably
          // ok seeing that the C++ types are different.
          //
          //
          if (src_m != 0)
          {
            string reason;

            if (transient (*src_m))
              reason = "transient";
            else if (inverse (*src_m))
              reason = "inverse";

            if (!reason.empty ())
            {
              error (e.loc)
                << "object data member '" << src_m->name () << "' specified "
                << "in db pragma column is " << reason << endl;
              throw operation_failed ();
            }

            if (!member_resolver::check_types (utype (*src_m), utype (m)))
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
        }
        // This member has no column information. If we are generating our
        // own query, try to find a member with the same (or similar) name
        // in one of the associated objects.
        //
        else if (query_.kind == view_query::condition)
        {
          view_objects& objs (view_.get<view_objects> ("objects"));

          assoc_members exact_members, pub_members;
          member_resolver resolver (exact_members, pub_members, m);

          for (view_objects::iterator i (objs.begin ()); i != objs.end (); ++i)
          {
            if (i->kind == view_object::object)
              resolver.traverse (*i);
          }

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

            throw operation_failed ();
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

            throw operation_failed ();
          }

          // Synthesize the column expression for this member.
          //
          assoc_member const& am (members.back ());

          column_expr& e (m.set ("column-expr", column_expr ()));
          e.push_back (column_expr_part ());
          column_expr_part& ep (e.back ());

          ep.kind = column_expr_part::reference;


          // If this object is polymorphic, then figure out which of the
          // bases this member comes from and use the corresponding table.
          //
          using semantics::class_;

          if (class_* root = polymorphic (*am.vo->obj))
          {
            class_* c (&static_cast<class_&> (am.m->scope ()));

            // If this member's class is not polymorphic (root uses reuse
            // inheritance), then use the root table.
            //
            if (!polymorphic (*c))
              c = root;

            // In a polymorphic hierarchy we have several tables and the
            // provided alias is used as a prefix together with the table
            // name to form the actual alias.
            //
            qname const& t (table_name (*c));

            if (am.vo->alias.empty ())
              ep.table = t;
            else
              ep.table = qname (am.vo->alias + "_" + t.uname ());
          }
          else
            ep.table = am.vo->alias.empty ()
              ? table_name (*am.vo->obj)
              : qname (am.vo->alias);

          ep.member_path.push_back (am.m);

          src_m = am.m;
        }

        // If we have the source member and don't have the type pragma of
        // our own, but the source member does, then copy the columnt type
        // over. In case the source member is a pointer, also check the id
        // member.
        //
        if (src_m != 0 && !m.count ("type"))
        {
          if (src_m->count ("type"))
            m.set ("column-type", src_m->get<string> ("column-type"));
          else if (semantics::class_* c = object_pointer (utype (*src_m)))
          {
            semantics::data_member& id (*id_member (*c));

            if (id.count ("type"))
              m.set ("column-type", id.get<string> ("column-type"));
          }
        }

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

          // First look for an exact match.
          //
          {
            member_.exact_ = true;
            member_.found_ = false;
            traverse (*vo.obj);
          }

          // If we didn't find an exact match, then look for a public
          // name match.
          //
          if (!member_.found_)
          {
            member_.exact_ = false;
            traverse (*vo.obj);
          }
        }

        virtual void
        traverse (type& c)
        {
          if (!object (c))
            return; // Ignore transient bases.

          names (c);

          // If we already found a match in one of the derived classes,
          // don't go into bases to get the standard "hiding" behavior.
          //
          if (!member_.found_)
            inherits (c);
        }

      public:
        static bool
        check_types (semantics::type& ot, semantics::type& vt)
        {
          using semantics::type;

          // Require that the types be the same sans the wrapping and
          // cvr-qualification. If the object member type is a pointer,
          // use the id type of the pointed-to object.
          //
          type* t1;

          if (semantics::class_* c = object_pointer (ot))
            t1 = &utype (*id_member (*c));
          else
            t1 = &ot;

          type* t2 (&vt);

          if (type* wt1 = context::wrapper (*t1))
            t1 = &utype (*wt1);

          if (type* wt2 = context::wrapper (*t2))
            t2 = &utype (*wt2);

          if (t1 != t2)
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
                type_ (utype (m))
          {
          }

          virtual void
          traverse (type& m)
          {
            if (exact_)
            {
              if (name_ == m.name () && check (m))
              {
                assoc_member am;
                am.m = &m;
                am.vo = vo_;
                members_.push_back (am);
                found_ = true;
              }
            }
            else
            {
              if (pub_name_ == context::current ().public_name (m) &&
                  check (m))
              {
                assoc_member am;
                am.m = &m;
                am.vo = vo_;
                pub_members_.push_back (am);
                found_ = true;
              }
            }
          }

          bool
          check (semantics::data_member& m)
          {
            // Make sure that the found node can possibly match.
            //
            if (context::transient (m) ||
                context::inverse (m) ||
                m.count ("polymorphic-ref"))
              return false;

            return check_types (utype (m), type_);
          }

          assoc_members& members_;
          assoc_members& pub_members_;

          string name_;
          string pub_name_;
          semantics::type& type_;

          view_object* vo_;
          bool exact_;
          bool found_;
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
          : std_string_ (0), std_string_hint_ (0)
      {
        // Resolve the std::string type node.
        //
        using semantics::scope;

        for (scope::names_iterator_pair ip (unit.find ("std"));
             ip.first != ip.second; ++ip.first)
        {
          if (scope* ns = dynamic_cast<scope*> (&ip.first->named ()))
          {
            scope::names_iterator_pair jp (ns->find ("string"));

            if (jp.first != jp.second)
            {
              std_string_ = dynamic_cast<semantics::type*> (
                &jp.first->named ());
              std_string_hint_ = &*jp.first;
              break;
            }
          }
        }

        assert (std_string_ != 0); // No std::string?

        *this >> member_names_ >> member_;
      }

      virtual void
      traverse (type& c)
      {
        class_kind_type k (class_kind (c));

        if (k == class_other)
          return;

        // Assign pointer.
        //
        if (k == class_object || k == class_view)
          assign_pointer (c);

        // Do some additional pre-processing.
        //
        if (k == class_object)
          traverse_object_pre (c);

        names (c);

        // Do some additional post-processing.
        //
        if (k == class_object)
          traverse_object_post (c);
        else if (k == class_view)
          traverse_view_post (c);
      }

      //
      // Object.
      //

      virtual void
      traverse_object_pre (type& c)
      {
        semantics::class_* poly_root (polymorphic (c));

        // Determine whether it is a session object.
        //
        if (!c.count ("session"))
        {
          // If this is a derived class in a polymorphic hierarchy,
          // then it should have the same session value as the root.
          //
          if (poly_root != 0 && poly_root != &c)
            c.set ("session", session (*poly_root));
          else
          {
            // See if any of the namespaces containing this class specify
            // the session value.
            //
            bool found (false);
            for (semantics::scope* s (&c.scope ());; s = &s->scope_ ())
            {
              using semantics::namespace_;

              namespace_* ns (dynamic_cast<namespace_*> (s));

              if (ns == 0)
                continue; // Some other scope.

              if (ns->extension ())
                ns = &ns->original ();

              if (ns->count ("session"))
              {
                c.set ("session", ns->get<bool> ("session"));
                found = true;
                break;
              }

              if (ns->global_scope ())
                break;
            }

            // If still not found, then use the default value.
            //
            if (!found)
              c.set ("session", options.generate_session ());
          }
        }

        if (session (c))
          features.session_object = true;

        if (poly_root != 0)
        {
          using namespace semantics;

          semantics::data_member& idm (*id_member (*poly_root));

          if (poly_root != &c)
          {
            // If we are a derived class in the polymorphic persistent
            // class hierarchy, then add a synthesized virtual pointer
            // member that points back to the root.
            //
            path const& f (idm.file ());
            size_t l (idm.line ()), col (idm.column ());

            semantics::data_member& m (
              unit.new_node<virtual_data_member> (f, l, col));

            // Make it the first member in the class.
            //
            node_position<type, scope::names_iterator> np (c, c.names_end ());
            unit.new_edge<semantics::names> (
              np, m, idm.name (), access::public_);

            // Use the raw pointer as this member's type.
            //
            if (!poly_root->pointed_p ())
            {
              // Create the pointer type in the graph. The pointer node
              // in GCC seems to always be present, even if not explicitly
              // used in the translation unit.
              //
              tree t (poly_root->tree_node ());
              tree ptr (TYPE_POINTER_TO (t));
              assert (ptr != 0);
              ptr = TYPE_MAIN_VARIANT (ptr);
              pointer& p (unit.new_node<pointer> (f, l, col, ptr));
              unit.insert (ptr, p);
              unit.new_edge<points> (p, *poly_root);
              assert (poly_root->pointed_p ());
            }

            unit.new_edge<belongs> (m, poly_root->pointed ().pointer ());

            m.set ("not-null", true);
            m.set ("deferred", false);
            m.set ("on-delete", sema_rel::foreign_key::cascade);

            // Mark it as a special kind of id.
            //
            m.set ("id", true);
            m.set ("polymorphic-ref", true);
          }
          else
          {
            // If we are a root of the polymorphic persistent class hierarchy,
            // then add a synthesized virtual member for the discriminator.
            // Use the location of the polymorphic pragma as the location of
            // this member.
            //
            location_t loc (c.get<location_t> ("polymorphic-location"));
            semantics::data_member& m (
              unit.new_node<virtual_data_member> (
                path (LOCATION_FILE (loc)),
                LOCATION_LINE (loc),
                LOCATION_COLUMN (loc)));

            // Insert it after the id member (or first if this id comes
            // from reuse-base).
            //
            node_position<type, scope::names_iterator> np (
              c, c.find (idm.named ()));
            unit.new_edge<semantics::names> (
              np, m, "typeid_", access::public_);

            belongs& edge (unit.new_edge<belongs> (m, *std_string_));
            edge.hint (*std_string_hint_);

            m.set ("readonly", true);
            m.set ("discriminator", true);

            c.set ("discriminator", &m);
          }
        }
      }

      virtual void
      traverse_object_post (type& c)
      {
        // Process indexes. Here we need to do two things: resolve member
        // names to member paths and assign names to unnamed indexes. We
        // are also going to handle the special container indexes.
        //
        indexes& ins (c.count ("index")
                      ? c.get<indexes> ("index")
                      : c.set ("index", indexes ()));

        for (indexes::iterator i (ins.begin ()); i != ins.end ();)
        {
          index& in (*i);

          // This should never happen since a db index pragma without
          // the member specifier will be treated as a member pragma.
          //
          assert (!in.members.empty ());

          // First resolve member names.
          //
          string tl;
          tree tn;
          cpp_ttype tt;

          index::members_type::iterator j (in.members.begin ());
          for (; j != in.members.end (); ++j)
          {
            index::member& im (*j);

            if (!im.path.empty ())
              continue; // Already resolved.

            try
            {
              lex_.start (im.name);
              tt = lex_.next (tl, &tn);

              // The name was already verified to be syntactically correct so
              // we don't need to do any error checking.
              //
              string name;   // Not used.
              cpp_ttype ptt; // Not used.
              tree decl (
                lookup::resolve_scoped_name (
                  lex_, tt, tl, tn, ptt, c.tree_node (), name, false));

              // Check that we have a data member.
              //
              if (TREE_CODE (decl) != FIELD_DECL)
              {
                error (im.loc) << "name '" << tl << "' in db pragma member "
                               << "does not refer to a data member" << endl;
                throw operation_failed ();
              }

              using semantics::data_member;

              data_member* m (dynamic_cast<data_member*> (unit.find (decl)));
              im.path.push_back (m);

              if (container (*m))
                break;

              // Resolve nested members if any.
              //
              for (; tt == CPP_DOT; tt = lex_.next (tl, &tn))
              {
                lex_.next (tl, &tn); // Get CPP_NAME.

                tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

                decl = lookup_qualified_name (
                  type, get_identifier (tl.c_str ()), false, false);

                if (decl == error_mark_node || TREE_CODE (decl) != FIELD_DECL)
                {
                  error (im.loc) << "name '" << tl << "' in db pragma member "
                                 << "does not refer to a data member" << endl;
                  throw operation_failed ();
                }

                m = dynamic_cast<data_member*> (unit.find (decl));
                im.path.push_back (m);

                if (container (*m))
                {
                  tt = lex_.next (tl, &tn); // Get CPP_DOT.
                  break; // Only breaks out of the inner loop.
                }
              }

              if (container (*m))
                break;
            }
            catch (lookup::invalid_name const&)
            {
              error (im.loc) << "invalid name in db pragma member" << endl;
              throw operation_failed ();
            }
            catch (lookup::unable_to_resolve const& e)
            {
              error (im.loc) << "unable to resolve name '" << e.name ()
                             << "' in db pragma member" << endl;
              throw operation_failed ();
            }
          }

          // Handle container indexes.
          //
          if (j != in.members.end ())
          {
            // Do some sanity checks.
            //
            if (in.members.size () != 1)
            {
              error (in.loc) << "multiple data members specified for a "
                             << "container index" << endl;
              throw operation_failed ();
            }

            if (tt != CPP_DOT || lex_.next (tl, &tn) != CPP_NAME ||
                (tl != "id" && tl != "index"))
            {
              error (j->loc) << ".id or .index special member expected in a "
                             << "container index" << endl;
              throw operation_failed ();
            }

            string n (tl);

            if (lex_.next (tl, &tn) != CPP_EOF)
            {
              error (j->loc) << "unexpected text after ." << n << " in "
                             << "db pragma member" << endl;
              throw operation_failed ();
            }

            // Move this index to the container member.
            //
            j->path.back ()->set (n + "-index", *i);
            i = ins.erase (i);
            continue;
          }

          // Now assign the name if the index is unnamed.
          //
          if (in.name.empty ())
          {
            // Make sure there is only one member.
            //
            if (in.members.size () > 1)
            {
              error (in.loc) << "unnamed index with more than one data "
                             << "member" << endl;
              throw operation_failed ();
            }

            // Generally, we want the index name to be based on the column
            // name. This is straightforward for single-column members. In
            // case of a composite member, we will need to use the column
            // prefix which is based on the data member name, unless
            // overridden by the user. In the latter case the prefix can
            // be empty, in which case we will just fall back on the
            // member's public name.
            //
            in.name = column_name (in.members.front ().path);

            if (in.name.empty ())
              in.name = public_name_db (*in.members.front ().path.back ());

            in.name = compose_name (in.name, "i");
          }

          ++i;
        }
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
      traverse_view_post (type& c)
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
            << "view '" << class_fq_name (c) << "' has an incomplete query "
            << "template and no associated objects" << endl;

          info (c.file (), c.line (), c.column ())
            << "use db pragma query to provide a complete query template"
            << endl;

          info (c.file (), c.line (), c.column ())
            << "or use db pragma object to associate one or more objects "
            << "with the view"
            << endl;

          throw operation_failed ();
        }

        // Resolve referenced objects from tree nodes to semantic graph
        // nodes.
        //
        view_alias_map& amap (c.set ("alias-map", view_alias_map ()));
        view_object_map& omap (c.set ("object-map", view_object_map ()));

        size_t& obj_count (c.set ("object-count", size_t (0)));
        size_t& tbl_count (c.set ("table-count", size_t (0)));

        if (has_o)
        {
          using semantics::class_;

          view_objects& objs (c.get<view_objects> ("objects"));

          for (view_objects::iterator i (objs.begin ()); i != objs.end (); ++i)
          {
            if (i->kind != view_object::object)
            {
              // Make sure we have join conditions for tables unless it
              // is the first entry.
              //
              if (i != objs.begin () && i->cond.empty ())
              {
                error (i->loc)
                  << "missing join condition in db pragma table" << endl;

                throw operation_failed ();
              }

              tbl_count++;
              continue;
            }
            else
              obj_count++;

            tree n (TYPE_MAIN_VARIANT (i->obj_node));

            if (TREE_CODE (n) != RECORD_TYPE)
            {
              error (i->loc)
                << "name '" << i->obj_name << "' in db pragma object does "
                << "not name a class" << endl;

              throw operation_failed ();
            }

            class_& o (dynamic_cast<class_&> (*unit.find (n)));

            if (!object (o))
            {
              error (i->loc)
                << "name '" << i->obj_name << "' in db pragma object does "
                << "not name a persistent class" << endl;

              info (o.file (), o.line (), o.column ())
                << "class '" << i->obj_name << "' is defined here" << endl;

              throw operation_failed ();
            }

            i->obj = &o;

            if (i->alias.empty ())
            {
              if (!omap.insert (view_object_map::value_type (n, &*i)).second)
              {
                error (i->loc)
                  << "persistent class '" << i->obj_name << "' is used in "
                  << "the view more than once" << endl;

                error (omap[n]->loc)
                  << "previously used here" << endl;

                info (i->loc)
                  << "use the alias clause to assign it a different name"
                  << endl;

                throw operation_failed ();
              }

              // Also add the bases of a polymorphic object.
              //
              class_* poly_root (polymorphic (o));

              if (poly_root != 0 && poly_root != &o)
              {
                for (class_* b (&polymorphic_base (o));;
                     b = &polymorphic_base (*b))
                {
                  view_object_map::value_type v (b->tree_node (), &*i);
                  if (!omap.insert (v).second)
                  {
                    error (i->loc)
                      << "base class '" << class_name (*b) << "' is "
                      << "used in the view more than once" << endl;

                    error (omap[v.first]->loc)
                      << "previously used here" << endl;

                    info (i->loc)
                      << "use the alias clause to assign it a different name"
                      << endl;

                    throw operation_failed ();
                  }

                  if (b == poly_root)
                    break;
                }
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

                throw operation_failed ();
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
                if (j->kind != view_object::object)
                  continue; // Skip tables.

                // First see if any of the objects that were specified
                // prior to this object point to it.
                //
                {
                  relationship_resolver r (rs, *i, true);
                  r.traverse (*j);
                }

                // Now see if this object points to any of the objects
                // specified prior to it.
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

                throw operation_failed ();
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

                throw operation_failed ();
              }

              // Synthesize the condition.
              //
              relationship const& r (rs.back ());

              string name (r.pointer->alias.empty ()
                           ? class_fq_name (*r.pointer->obj)
                           : r.pointer->alias);
              name += "::";
              name += r.name;

              lex_.start (name);

              string t;
              for (cpp_ttype tt (lex_.next (t));
                   tt != CPP_EOF;
                   tt = lex_.next (t))
              {
                cxx_token ct (lex_.location (), tt);
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
                               bool forward)
            // Look in polymorphic bases only for previously-associated
            // objects since backward pointers from bases will result in
            // the pathological case (we will have to join the base table
            // first, which means we will get both bases and derived objects
            // instead of just derived).
            //
            : object_members_base (false, false, true, forward),
              relationships_ (rs),
              // Ignore self-references if we are looking for backward
              // pointers since they were already added to the list in
              // the previous pass.
              //
              self_pointer_ (forward),
              pointer_ (0),
              pointee_ (pointee)
        {
        }

        void
        traverse (view_object& pointer)
        {
          pointer_ = &pointer;
          object_members_base::traverse (*pointer.obj);
        }

        virtual void
        traverse_pointer (semantics::data_member& m, semantics::class_& c)
        {
          // Ignore polymorphic id references.
          //
          if (m.count ("polymorphic-ref"))
            return;

          // Ignore inverse sides of the same relationship to avoid
          // phony conflicts caused by the direct side that will end
          // up in the relationship list as well.
          //
          if (inverse (m))
            return;

          // Ignore self-pointers if requested.
          //
          if (!self_pointer_ && pointer_->obj == &c)
            return;

          if (pointee_.obj == &c)
          {
            relationships_.push_back (relationship ());
            relationships_.back ().member = &m;
            relationships_.back ().name = member_prefix_ + m.name ();
            relationships_.back ().pointer = pointer_;
            relationships_.back ().pointee = &pointee_;
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
            if (!self_pointer_ && pointer_->obj == c)
              return;

            if (pointee_.obj == c)
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
          string const& type (class_fq_name (c));

          tree decl (0);          // Resolved template node.
          string decl_name;       // User-provided template name.
          tree resolve_scope (0); // Scope in which we resolve names.

          class_pointer const* cp (0);
          bool cp_template (false);

          if (c.count ("pointer"))
          {
            cp = &c.get<class_pointer> ("pointer");
          }
          // If we are a derived type in polymorphic hierarchy, then use
          // our root's pointer type by default.
          //
          else if (semantics::class_* r = polymorphic (c))
          {
            if (&c != r && r->count ("pointer-template"))
              cp = r->get<class_pointer const*> ("pointer-template");
          }

          if (cp != 0)
          {
            string const& p (cp->name);

            if (p == "*")
            {
              ptr = type + "*";
              cp_template = true;
            }
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
              decl = resolve_name (p, cp->scope, true);
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
                ptr = p + "< " + type + " >";
                decl_name = p;
                cp_template = true;
              }
              else
              {
                error (cp->loc)
                  << "name '" << p << "' specified with db pragma pointer "
                  << "does not name a type or a template" << endl;

                throw operation_failed ();
              }
            }

            // Resolve scope is the scope of the pragma.
            //
            resolve_scope = cp->scope;
            loc = cp->loc;
          }
          else
          {
            // See if any of the namespaces containing this class specify
            // a pointer.
            //
            for (semantics::scope* s (&c.scope ());; s = &s->scope_ ())
            {
              using semantics::namespace_;

              namespace_* ns (dynamic_cast<namespace_*> (s));

              if (ns == 0)
                continue; // Some other scope.

              if (ns->extension ())
                ns = &ns->original ();

              if (!ns->count ("pointer"))
              {
                if (ns->global_scope ())
                  break;
                else
                  continue;
              }

              cp = &ns->get<class_pointer> ("pointer");
              string const& p (cp->name);

              // Namespace-specified pointer can only be '*' or are template.
              //
              if (p == "*")
                ptr = type + "*";
              else if (p[p.size () - 1] == '*')
              {
                error (cp->loc)
                  << "name '" << p << "' specified with db pragma pointer "
                  << "at namespace level cannot be a raw pointer" << endl;
              }
              else if (p.find ('<') != string::npos)
              {
                error (cp->loc)
                  << "name '" << p << "' specified with db pragma pointer "
                  << "at namespace level cannot be a template-id" << endl;
              }
              else
              {
                // Resolve this name and make sure it is a template.
                //
                decl = resolve_name (p, cp->scope, true);
                int tc (TREE_CODE (decl));

                if (tc == TEMPLATE_DECL && DECL_CLASS_TEMPLATE_P (decl))
                {
                  ptr = p + "< " + type + " >";
                  decl_name = p;
                }
                else
                {
                  error (cp->loc)
                    << "name '" << p << "' specified with db pragma pointer "
                    << "does not name a template" << endl;
                }
              }

              if (ptr.empty ())
                throw operation_failed ();

              cp_template = true;

              // Resolve scope is the scope of the pragma.
              //
              resolve_scope = cp->scope;
              loc = cp->loc;
              break;
            }

            // Use the default pointer.
            //
            if (ptr.empty ())
            {
              string const& p (options.default_pointer ());

              if (p == "*")
                ptr = type + "*";
              else
              {
                ptr = p + "< " + type + " >";
                decl_name = p;
              }

              // Resolve scope is the scope of the class.
              //
              resolve_scope = c.scope ().tree_node ();
            }
          }

          // If this class is a root of a polymorphic hierarchy, then cache
          // the pointer template so that we can use it for derived classes.
          //
          if (cp != 0 && cp_template && polymorphic (c) == &c)
            c.set ("pointer-template", cp);

          // Check if we are using TR1.
          //
          if (decl != 0 || !decl_name.empty ())
          {
            bool& tr1 (features.tr1_pointer);
            bool& boost (features.boost_pointer);

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

                throw operation_failed ();
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
            lex_.start (ptr);
            ptr.clear ();

            string t;
            bool punc (false);
            bool scoped (false);

            for (cpp_ttype tt (lex_.next (t));
                 tt != CPP_EOF;
                 tt = lex_.next (t))
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
            throw operation_failed ();
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


          throw operation_failed ();
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

          throw operation_failed ();
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
          string tl;
          tree tn;
          cpp_ttype tt, ptt;

          nlex_.start (qn);
          tt = nlex_.next (tl, &tn);

          string name;
          return lookup::resolve_scoped_name (
            nlex_, tt, tl, tn, ptt, scope, name, is_type);
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
      cxx_string_lexer lex_;
      cxx_string_lexer nlex_; // Nested lexer.

      data_member member_;
      traversal::names member_names_;

      semantics::type* std_string_;
      semantics::names* std_string_hint_;
    };
  }

  void
  process ()
  {
    context ctx;

    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (true);
    traversal::namespace_ ns;
    class_ c;

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (true);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

    unit.dispatch (ctx.unit);
  }
}
