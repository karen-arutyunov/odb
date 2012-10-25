// file      : odb/processor.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <iostream>

#include <odb/common.hxx>
#include <odb/lookup.hxx>
#include <odb/context.hxx>
#include <odb/cxx-lexer.hxx>
#include <odb/processor.hxx>
#include <odb/diagnostics.hxx>

#include <odb/relational/processor.hxx>

using namespace std;

namespace
{
  struct data_member: traversal::data_member, context
  {
    virtual void
    traverse (semantics::data_member& m)
    {
      if (transient (m))
        return;

      process_access (m, "get");
      process_access (m, "set");
    }

    //
    // Process member access expressions.
    //

    enum found_type
    {
      found_none,
      found_some, // Found something but keep looking for a better one.
      found_best
    };

    // Check if a function is a suitable accessor for this member.
    //
    found_type
    check_accessor (semantics::data_member& m,
                    tree f,
                    string const& n,
                    member_access& ma,
                    bool strict)
    {
      // Must be const.
      //
      if (!DECL_CONST_MEMFUNC_P (f))
        return found_none;

      // Accessor is a function with no arguments (other than 'this').
      //
      if (DECL_CHAIN (DECL_ARGUMENTS (f)) != NULL_TREE)
        return found_none;

      // Note that to get the return type we have to use
      // TREE_TYPE(TREE_TYPE()) and not DECL_RESULT, as
      // suggested in the documentation.
      //
      tree r (TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (f))));
      int tc (TREE_CODE (r));

      // In the strict mode make sure the function returns for non-array
      // types a value or a (const) reference to the member type and for
      // array types a (const) pointer to element type. In the lax mode
      // we just check that the return value is not void.
      //
      if (strict)
      {
        semantics::type& t (utype (m));
        semantics::array* ar (dynamic_cast<semantics::array*> (&t));

        if (ar != 0 && tc != POINTER_TYPE)
          return found_none;

        tree bt (ar != 0 || tc == REFERENCE_TYPE ? TREE_TYPE (r) : r);
        tree bt_mv (TYPE_MAIN_VARIANT (bt));

        if ((ar != 0 ? ar->base_type () : t).tree_node () != bt_mv)
          return found_none;
      }
      else if (r == void_type_node)
        return found_none;

      cxx_tokens& e (ma.expr);
      e.push_back (cxx_token (0, CPP_KEYWORD, "this"));
      e.push_back (cxx_token (0, CPP_DOT));
      e.push_back (cxx_token (0, CPP_NAME, n));
      e.push_back (cxx_token (0, CPP_OPEN_PAREN, n));
      e.push_back (cxx_token (0, CPP_CLOSE_PAREN, n));

      // See if it returns by value.
      //
      ma.by_value = (tc != REFERENCE_TYPE && tc != POINTER_TYPE);

      return found_best;
    }

    // Check if a function is a suitable modifier for this member.
    //
    found_type
    check_modifier (semantics::data_member& m,
                    tree f,
                    string const& n,
                    member_access& ma,
                    bool strict)
    {
      tree a (DECL_ARGUMENTS (f));
      a = DECL_CHAIN (a); // Skip this.

      // For a modifier, it can either be a function that returns a non-
      // const reference (or non-const pointer, in case the member is an
      // array) or a by-value modifier that sets a new value. If both are
      // available, we prefer the former for efficiency.
      //
      cxx_tokens& e (ma.expr);
      semantics::type& t (utype (m));
      semantics::array* ar (dynamic_cast<semantics::array*> (&t));

      if (a == NULL_TREE)
      {
        // Note that to get the return type we have to use
        // TREE_TYPE(TREE_TYPE()) and not DECL_RESULT, as
        // suggested in the documentation.
        //
        tree r (TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (f))));
        int tc (TREE_CODE (r));

        // By-reference modifier. Should return a reference or a pointer.
        //
        if (tc != (ar != 0 ? POINTER_TYPE : REFERENCE_TYPE))
          return found_none;

        // The base type should not be const and, in strict mode, should
        // match the member type.
        //
        tree bt (TREE_TYPE (r));

        if (CP_TYPE_CONST_P (bt))
          return found_none;

        tree bt_mv (TYPE_MAIN_VARIANT (bt));

        if (strict && (ar != 0 ? ar->base_type () : t).tree_node () != bt_mv)
          return found_none;

        e.clear (); // Could contain by value modifier.
        e.push_back (cxx_token (0, CPP_KEYWORD, "this"));
        e.push_back (cxx_token (0, CPP_DOT));
        e.push_back (cxx_token (0, CPP_NAME, n));
        e.push_back (cxx_token (0, CPP_OPEN_PAREN, n));
        e.push_back (cxx_token (0, CPP_CLOSE_PAREN, n));

        return found_best;
      }
      // Otherwise look for a by value modifier, which is a function
      // with a single argument.
      //
      else if (DECL_CHAIN (a) == NULL_TREE)
      {
        // In the lax mode any function with a single argument works
        // for us. And we don't care what it returns.
        //
        if (strict)
        {
          // In the strict mode make sure the argument matches the
          // member. This is exactly the same logic as in accessor
          // with regards to arrays, references, etc.
          //
          tree at (TREE_TYPE (a));
          int tc (TREE_CODE (at));

          if (ar != 0 && tc != POINTER_TYPE)
            return found_none;

          tree bt (ar != 0 || tc == REFERENCE_TYPE ? TREE_TYPE (at) : at);
          tree bt_mv (TYPE_MAIN_VARIANT (bt));

          if ((ar != 0 ? ar->base_type () : t).tree_node () != bt_mv)
            return found_none;
        }

        if (e.empty ())
        {
          e.push_back (cxx_token (0, CPP_KEYWORD, "this"));
          e.push_back (cxx_token (0, CPP_DOT));
          e.push_back (cxx_token (0, CPP_NAME, n));
          e.push_back (cxx_token (0, CPP_OPEN_PAREN, n));
          e.push_back (cxx_token (0, CPP_QUERY));
          e.push_back (cxx_token (0, CPP_CLOSE_PAREN, n));

          // Continue searching in case there is version that returns a
          // non-const reference which we prefer for efficiency.
          //
          return found_some;
        }
        else
          return found_none; // We didn't find anything better.
      }

      return found_none;
    }

    void
    process_access (semantics::data_member& m, std::string const& k)
    {
      bool virt (m.count ("virtual"));

      // Ignore certain special virtual members.
      //
      if (virt && (m.count ("polymorphic-ref") || m.count ("discriminator")))
        return;

      char const* kind (k == "get" ? "accessor" : "modifier");
      semantics::class_& c (dynamic_cast<semantics::class_&> (m.scope ()));

      // If we don't have an access expression, try to come up with
      // one.
      //
      if (!m.count (k))
      {
        found_type found (found_none);
        semantics::access const& a (m.named ().access ());
        member_access& ma (m.set (k, member_access (m.location (), true)));

        // If this member is not virtual and is either public or if we
        // are a friend of this class, then go for the member directly.
        //
        if (!virt && (a == semantics::access::public_ ||
                      c.get<bool> ("friend")))
        {
          ma.expr.push_back (cxx_token (0, CPP_KEYWORD, "this"));
          ma.expr.push_back (cxx_token (0, CPP_DOT));
          ma.expr.push_back (cxx_token (0, CPP_NAME, m.name ()));
          found = found_best;
        }

        // Otherwise, try to find a suitable accessor/modifier.
        //

        // First try the original name. If that doesn't produce anything,
        // then try the public name.
        //
        bool t (k == "get"
                ? options.accessor_regex_trace ()
                : options.modifier_regex_trace ());
        regex_mapping const& re (
          k == "get" ? accessor_regex : modifier_regex);

        for (unsigned short j (0); found != found_best && j != 2; ++j)
        {
          string b (j == 0 ? m.name () : public_name (m, false));

          // Skip the second pass if original and public names are the same.
          //
          if (j == 1 && b == m.name ())
            continue;

          if (t)
            cerr << kind << (j == 0 ? " original" : " public")
                 << " name '" << b << "'" << endl;

          for (regex_mapping::const_iterator i (re.begin ());
               found != found_best && i != re.end ();
               ++i)
          {
            if (t)
              cerr << "try: '" << i->regex () << "' : ";

            if (!i->match (b))
            {
              if (t)
                cerr << '-' << endl;
              continue;
            }

            string n (i->replace (b));

            if (t)
              cerr << "'" << n << "' : ";

            tree decl (
              lookup_qualified_name (
                c.tree_node (), get_identifier (n.c_str ()), false, false));

            if (decl == error_mark_node || TREE_CODE (decl) != BASELINK)
            {
              if (t)
                cerr << '-' << endl;
              continue;
            }

            // OVL_* macros work for both FUNCTION_DECL and OVERLOAD.
            //
            for (tree o (BASELINK_FUNCTIONS (decl));
                 o != 0;
                 o = OVL_NEXT (o))
            {
              tree f (OVL_CURRENT (o));

              // We are only interested in public non-static member
              // functions. Note that TREE_PUBLIC() returns something
              // other than what we need.
              //
              if (!DECL_NONSTATIC_MEMBER_FUNCTION_P (f) ||
                  TREE_PRIVATE (f) || TREE_PROTECTED (f))
                continue;

              found_type r (k == "get"
                            ? check_accessor (m, f, n, ma, true)
                            : check_modifier (m, f, n, ma, true));

              if (r != found_none)
              {
                // Update the location of the access expression to point
                // to this function.
                //
                ma.loc = location (DECL_SOURCE_LOCATION (f));
                found = r;
              }
            }

            if (t)
              cerr << (found != found_none ? '+' : '-') << endl;
          }
        }

        // If that didn't work then the generated code won't be able
        // to access this member.
        //
        if (found == found_none)
        {
          location const& l (m.location ());

          if (virt)
          {
            error (l) << "no suitable " << kind << " function could be "
                      << "automatically found for virtual data member '"
                      << m.name () << "'" << endl;

            info (l)  << "use '#pragma db " << k << "' to explicitly "
                      << "specify the " << kind << " function or "
                      << "expression" << endl;
          }
          else
          {
            error (l) << "data member '" << m.name () << "' is "
                      << a.string () << " and no suitable " << kind
                      << " function could be automatically found" << endl;

            info (l)  << "consider making class 'odb::access' a friend of "
                      << "class '" << class_name (c) << "'" << endl;

            info (l)  << "or use '#pragma db " << k << "' to explicitly "
                      << "specify the " << kind << " function or "
                      << "expression" << endl;
          }

          throw operation_failed ();
        }
      }

      member_access& ma (m.get<member_access> (k));
      cxx_tokens& e (ma.expr);

      // If it is just a name, resolve it and convert to an appropriate
      // expression.
      //
      if (e.size () == 1 && e.back ().type == CPP_NAME)
      {
        string n (e.back ().literal);
        e.clear ();

        tree decl (
          lookup_qualified_name (
            c.tree_node (), get_identifier (n.c_str ()), false, false));

        if (decl == error_mark_node)
        {
          error (ma.loc) << "unable to resolve data member or function "
                         << "name '" << n << "'" << endl;
          throw operation_failed ();
        }

        switch (TREE_CODE (decl))
        {
        case FIELD_DECL:
          {
            e.push_back (cxx_token (0, CPP_KEYWORD, "this"));
            e.push_back (cxx_token (0, CPP_DOT));
            e.push_back (cxx_token (0, CPP_NAME, n));
            break;
          }
        case BASELINK:
          {
            // OVL_* macros work for both FUNCTION_DECL and OVERLOAD.
            //
            for (tree o (BASELINK_FUNCTIONS (decl));
                 o != 0;
                 o = OVL_NEXT (o))
            {
              tree f (OVL_CURRENT (o));

              // We are only interested in non-static member functions.
              //
              if (!DECL_NONSTATIC_MEMBER_FUNCTION_P (f))
                continue;

              if ((k == "get"
                   ? check_accessor (m, f, n, ma, false)
                   : check_modifier (m, f, n, ma, false)) == found_best)
                break;
            }

            if (e.empty ())
            {
              error (ma.loc) << "unable to find suitable " << kind
                             << " function '" << n << "'" << endl;
              throw operation_failed ();
            }
            break;
          }
        default:
          {
            error (ma.loc) << "name '" << n << "' does not refer to a data "
                           << "member or function" << endl;
            throw operation_failed ();
          }
        }
      }

      // If there is no 'this' keyword, then add it as a prefix.
      //
      {
        bool t (false);
        for (cxx_tokens::iterator i (e.begin ()); i != e.end (); ++i)
        {
          if (i->type == CPP_KEYWORD && i->literal == "this")
          {
            t = true;
            break;
          }
        }

        if (!t)
        {
          e.insert (e.begin (), cxx_token (0, CPP_DOT));
          e.insert (e.begin (), cxx_token (0, CPP_KEYWORD, "this"));
        }
      }

      // Check that there is no placeholder in the accessor expression.
      //
      if (k == "get" && ma.placeholder ())
      {
        error (ma.loc) << "(?) placeholder in the accessor expression"
                       << endl;
        throw operation_failed ();
      }

      // Check that the member type is default-constructible if we
      // have a by value modifier.
      //
      if (k == "set" && ma.placeholder ())
      {
        semantics::class_* c (dynamic_cast<semantics::class_*> (&utype (m)));

        // Assume all other types are default-constructible.
        //
        if (c != 0)
        {
          // If this type is a class template instantiation, then make
          // sure it is instantiated. While types used in real members
          // will be instantiated, this is not necessarily the case for
          // virtual members. Without the instantiation we won't be able
          // to detect whether the type has the default ctor.
          //
          // It would have been cleaner to do it in post_process_pragmas()
          // but there we don't yet know whether we need the default ctor.
          // And it is a good idea not to require instantiability unless
          // we really need it.
          //
          tree type (c->tree_node ());

          if (!COMPLETE_TYPE_P (type) &&
              CLASSTYPE_TEMPLATE_INSTANTIATION (type))
          {
            // Reset input location so that we get nice diagnostics in
            // case of an error. Use the location of the virtual pragma.
            //
            location_t loc (m.get<location_t> ("virtual-location"));
            input_location = loc;

            if (instantiate_class_template (type) == error_mark_node ||
                errorcount != 0 ||
                !COMPLETE_TYPE_P (type))
            {
              error (loc) << "unable to instantiate virtual data member " <<
                "type" << endl;
              throw operation_failed ();
            }
          }

          if (!c->default_ctor ())
          {
            error (ma.loc) << "modifier expression requires member type " <<
              "to be default-constructible" << endl;
            throw operation_failed ();
          }
        }
      }
    }
  };

  struct class_: traversal::class_, context
  {
    class_ ()
        : std_string_ (0), std_string_hint_ (0), access_ (0)
    {
      *this >> member_names_ >> member_;

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

      // Resolve odb::access, if any.
      //
      tree odb = lookup_qualified_name (
        global_namespace, get_identifier ("odb"), false, false);

      if (odb != error_mark_node)
      {
        access_ = lookup_qualified_name (
          odb, get_identifier ("access"), true, false);

        access_ = (access_ != error_mark_node ? TREE_TYPE (access_) : 0);
      }
    }

    virtual void
    traverse (type& c)
    {
      class_kind_type k (class_kind (c));

      if (k == class_other)
        return;

      // Check if odb::access is a friend of this class.
      //
      c.set ("friend", access_ != 0 && is_friend (c.tree_node (), access_));

      // Assign pointer.
      //
      if (k == class_object || k == class_view)
        assign_pointer (c);

      if (k == class_object)
        traverse_object (c);

      names (c);
    }

    //
    // Object.
    //

    virtual void
    traverse_object (type& c)
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
            unit.new_node<semantics::data_member> (f, l, col, tree (0)));
          m.set ("virtual", true);

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
            unit.new_node<semantics::data_member> (
              path (LOCATION_FILE (loc)),
              LOCATION_LINE (loc),
              LOCATION_COLUMN (loc),
              tree (0)));
          m.set ("virtual", true);

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
    data_member member_;
    traversal::names member_names_;

    cxx_string_lexer lex_;
    cxx_string_lexer nlex_; // Nested lexer.

    semantics::type* std_string_;
    semantics::names* std_string_hint_;

    tree access_; // odb::access node.
  };
}

void processor::
process (options const& ops,
         features& f,
         semantics::unit& unit,
         semantics::path const&)
{
  try
  {
    auto_ptr<context> ctx (create_context (cerr, unit, ops, f, 0));

    // Common processing.
    //
    {
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

      unit.dispatch (ctx->unit);
    }

    // Database-specific processing.
    //
    switch (ops.database ()[0])
    {
    case database::common:
      {
        break;
      }
    case database::mssql:
    case database::mysql:
    case database::oracle:
    case database::pgsql:
    case database::sqlite:
      {
        relational::process ();
        break;
      }
    }
  }
  catch (operation_failed const&)
  {
    // Processing failed. Diagnostics has already been issued.
    //
    throw failed ();
  }
}
