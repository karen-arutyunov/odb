// file      : odb/plugin.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <gcc.hxx> // Keep it first.

#include <set>
#include <map>
#include <string>
#include <memory>  // std::auto_ptr
#include <cassert>
#include <sstream>

#include <semantics.hxx>

#ifndef LOCATION_COLUMN
#define LOCATION_COLUMN(LOC) (expand_location (LOC).column)
#endif

#ifndef DECL_SOURCE_COLUMN
#define DECL_SOURCE_COLUMN(NODE) LOCATION_COLUMN (DECL_SOURCE_LOCATION (NODE))
#endif

using namespace std;
using namespace semantics;

int plugin_is_GPL_compatible;

class parser
{
public:
  typedef semantics::access access;

  parser ()
  {
  }

  auto_ptr<semantics::unit>
  parse (tree global_scope, path const& main_file)
  {
    /*
    for (size_t i (0); i < line_table->used; ++i)
    {
      const line_map* m (line_table->maps + i);
      warning (0, G_ ("line map to %s reason %d"), m->to_file, m->reason);
    }

    for (size_t i (0); i < line_table->used; ++i)
    {
      const line_map* m (line_table->maps + i);

      if (MAIN_FILE_P (m) || m->reason != LC_ENTER)
        continue;

      const line_map* i (INCLUDED_FROM (line_table, m));

      if (!MAIN_FILE_P (i))
        continue;

      warning (0, G_ ("#includ %s at %s:%d"),
               m->to_file,
               i->to_file,
               LAST_SOURCE_LINE (i));
    }
    */

    auto_ptr<unit> u (new unit (main_file));
    unit_ = u.get ();
    scope_ = unit_;

    // Define fundamental types.
    //
    define_fund<fund_void> (void_type_node);
    define_fund<fund_bool> (boolean_type_node);
    define_fund<fund_char> (char_type_node);
    define_fund<fund_wchar> (wchar_type_node);
    define_fund<fund_signed_char> (signed_char_type_node);
    define_fund<fund_unsigned_char> (unsigned_char_type_node);
    define_fund<fund_short> (short_integer_type_node);
    define_fund<fund_unsigned_short> (short_unsigned_type_node);
    define_fund<fund_int> (integer_type_node);
    define_fund<fund_unsigned_int> (unsigned_type_node);
    define_fund<fund_long> (long_integer_type_node);
    define_fund<fund_unsigned_long> (long_unsigned_type_node);
    define_fund<fund_long_long> (long_long_integer_type_node);
    define_fund<fund_unsigned_long_long> (long_long_unsigned_type_node);
    define_fund<fund_float> (float_type_node);
    define_fund<fund_double> (double_type_node);
    define_fund<fund_long_double> (long_double_type_node);

    // First collect all the declarations we are interested in
    // in the line-decl map so that they appear in the source
    // code order.
    //
    collect (global_scope);
    emit ();

    return u;
  }

private:
  void
  collect (tree ns)
  {
    cp_binding_level* level = NAMESPACE_LEVEL (ns);
    tree decl = level->names;

    // Collect declarations.
    //
    for (; decl != NULL_TREE; decl = TREE_CHAIN (decl))
    {
      if (DECL_IS_BUILTIN (decl))
        continue;

      switch (TREE_CODE (decl))
      {
      case TYPE_DECL:
        {
          /*
          location_t l (DECL_SOURCE_LOCATION (decl));

          if (l > BUILTINS_LOCATION)
          {
            warning (0, G_ ("decl in %s"), DECL_SOURCE_FILE (decl));

            const line_map* map = linemap_lookup (line_table, l);

            if (map != 0 && !MAIN_FILE_P (map))
            {
              map = INCLUDED_FROM (line_table, map);
              warning (0, G_ ("included from %s:%d"),
                       map->to_file,
                       LAST_SOURCE_LINE (map));
            }
          }
          */

          if (DECL_NAME (decl) != NULL_TREE)
            decls_.insert (decl);

          /*
          tree type (TREE_TYPE (decl));

          if (TREE_CODE (type) == RECORD_TYPE)
          {
            tree name (DECL_NAME (decl));

            if (name != NULL_TREE)
            {
              if (DECL_ARTIFICIAL (decl))
              {
                // If we have an anonymous class typedef, use the user-
                // supplied name instead of the synthesized one. ARM
                // says that in typedef struct {} S; S becomes struct's
                // name.
                //
                if (ANON_AGGRNAME_P (name))
                {
                  tree d (TYPE_NAME (type));

                  if (d != NULL_TREE &&
                      !DECL_ARTIFICIAL (d) &&
                      DECL_NAME (d) != NULL_TREE &&
                      !ANON_AGGRNAME_P (DECL_NAME (d)))
                  {
                    decls_.insert (d);
                  }
                }
                else
                  decls_.insert (decl);
              }
            }
          }
          */

          break;
        }
      default:
        {
          /*
          if (!DECL_IS_BUILTIN (decl))
          {
            tree name = DECL_NAME (decl);

            if (name != NULL_TREE)
            {
              warning (0, G_ ("some declaration %s in %s:%i"),
                       IDENTIFIER_POINTER (name),
                       DECL_SOURCE_FILE (decl),
                       DECL_SOURCE_LINE (decl));
            }
            else
            {
              warning (0, G_ ("some unnamed declaration in %s:%i"),
                       DECL_SOURCE_FILE (decl),
                       DECL_SOURCE_LINE (decl));
            }
          }
          */
          break;
        }
      }
    }

    // Traverse namespaces.
    //
    for(decl = level->namespaces; decl != NULL_TREE; decl = TREE_CHAIN (decl))
    {
      // !DECL_NAMESPACE_STD_P (decl)

      if (!DECL_IS_BUILTIN (decl))
      {
        tree name = DECL_NAME (decl);

        warning (0, G_ ("namespace declaration %s in %s:%i"),
                 name ? IDENTIFIER_POINTER (name) : "<anonymous>",
                 DECL_SOURCE_FILE (decl),
                 DECL_SOURCE_LINE (decl));

        collect (decl);
      }
    }
  }

  void
  emit ()
  {
    for (decl_set::const_iterator i (decls_.begin ()), e (decls_.end ());
         i != e; ++i)
    {
      tree decl (*i);

      // Get this declaration's namespace and unwind our scope until
      // we find a common prefix of namespaces.
      //
      string pfx;
      string ns (fq_scope (decl));

      for (pfx = scope_->fq_name (); !pfx.empty (); pfx = scope_->fq_name ())
      {
        if (ns.compare (0, pfx.size (), pfx) == 0)
          break;

        scope_ = &scope_->scope_ ();
      }

      // Build the rest of the namespace hierarchy for this declaration.
      //
      if (ns != pfx)
      {
        path f (DECL_SOURCE_FILE (decl));
        size_t l (DECL_SOURCE_LINE (decl));
        size_t c (DECL_SOURCE_COLUMN (decl));

        for (size_t b (pfx.size () + 2), e (ns.find ("::", b));
             b != string::npos;)
        {
          string n (ns, b, e == string::npos ? e : e - b);

          warning (0, G_ ("creating namespace %s for %s:%d"),
                   n.c_str (),
                   DECL_SOURCE_FILE (decl),
                   DECL_SOURCE_LINE (decl));

          // Use the declarations's file, line, and column as an
          // approximation for this namespace origin.
          //
          namespace_& node (unit_->new_node<namespace_> (f, l, c));
          unit_->new_edge<defines> (*scope_, node, n);
          scope_ = &node;

          if (e == string::npos)
            b = e;
          else
          {
            b = e + 2;
            e = ns.find ("::", b);
          }
        }
      }

      switch (TREE_CODE (decl))
      {
      case TYPE_DECL:
        {
          tree t (TREE_TYPE (decl));
          tree decl_name (DECL_NAME (decl));
          char const* name (IDENTIFIER_POINTER (decl_name));

          if (DECL_ARTIFICIAL (decl) && TREE_CODE (t) == RECORD_TYPE)
          {
            // If we have an anonymous class typedef, use the user-
            // supplied name instead of the synthesized one. ARM
            // says that in typedef struct {} S; S becomes struct's
            // name.
            //
            if (ANON_AGGRNAME_P (decl_name))
            {
              tree d (TYPE_NAME (t));

              if (d != NULL_TREE &&
                  !DECL_ARTIFICIAL (d) &&
                  DECL_NAME (d) != NULL_TREE &&
                  !ANON_AGGRNAME_P (DECL_NAME (d)))
              {
                decl = d;
                decl_name = DECL_NAME (decl);
                name = IDENTIFIER_POINTER (decl_name);
              }
              else
              {
                // This type has only the synthesized name which means that
                // it is either typedef'ed as a derived type or it is used
                // to declare a varibale or similar. The first case will be
                // covered by the typedef handling code below. The second
                // case we don't care about.
                //
                break;
              }
            }

            path file (DECL_SOURCE_FILE (decl));
            size_t line (DECL_SOURCE_LINE (decl));
            size_t clmn (DECL_SOURCE_COLUMN (decl));

            warning (0, G_ ("start class declaration %s in %s:%d"),
                     name,
                     DECL_SOURCE_FILE (decl),
                     DECL_SOURCE_LINE (decl));

            class_& node (emit_class (t, file, line, clmn));

            if (COMPLETE_TYPE_P (t))
              unit_->new_edge<defines> (*scope_, node, name);
            else
              unit_->new_edge<declares> (*scope_, node, name);

            warning (0, G_ ("end class declaration %s (%p) in %s:%d"),
                     name,
                     &node,
                     DECL_SOURCE_FILE (decl),
                     DECL_SOURCE_LINE (decl));
          }
          else
          {
            // Normal typedef. We need to detect and ignore the anonymous
            // class typedef case described above since we already used
            // this name to define the class.
            //
            if (TREE_CODE (t) == RECORD_TYPE &&
                TYPE_NAME (TYPE_MAIN_VARIANT (t)) == decl)
              break;

            path f (DECL_SOURCE_FILE (decl));
            size_t l (DECL_SOURCE_LINE (decl));
            size_t c (DECL_SOURCE_COLUMN (decl));

            type& node (emit_type (t, f, l, c));
            unit_->new_edge<typedefs> (*scope_, node, name);

            string s (emit_type_name (t, false));

            warning (0, G_ ("typedef declaration %s (%p) -> %s  in %s:%i"),
                     s.c_str (),
                     &node,
                     name,
                     DECL_SOURCE_FILE (decl),
                     DECL_SOURCE_LINE (decl));
          }

          break;
        }
      case TEMPLATE_DECL:
        {
          break;
        }
      }
    }
  }

  class_&
  emit_class (tree c,
              path const& file,
              size_t line,
              size_t clmn,
              bool stub = false)
  {
    c = TYPE_MAIN_VARIANT (c);

    // See if there is a stub already for this type.
    //
    class_* class_node (0);

    if (node* n = unit_->find (c))
    {
      class_node = &dynamic_cast<class_&> (*n);
    }
    else
    {
      class_node = &unit_->new_node<class_> (file, line, clmn);
      unit_->insert (c, *class_node);
    }

    if (stub || !COMPLETE_TYPE_P (c))
      return *class_node;

    // Traverse base information.
    //
    tree bis (TYPE_BINFO (c));
    size_t n (bis ? BINFO_N_BASE_BINFOS (bis) : 0);

    for (size_t i (0); i < n; i++)
    {
      tree bi (BINFO_BASE_BINFO (bis, i));
      access a (access::public_);

      if (BINFO_BASE_ACCESSES (bis))
      {
        tree ac (BINFO_BASE_ACCESS (bis, i));

        if (ac == NULL_TREE || ac == access_public_node)
        {
          a = access::public_;
        }
        else if (ac == access_protected_node)
        {
          a = access::protected_;
        }
        else
        {
          assert (ac == access_private_node);
          a = access::private_;
        }
      }

      bool virt (BINFO_VIRTUAL_P (bi));
      tree base (BINFO_TYPE (bi));
      tree base_decl (TYPE_NAME (base)); // Typedef decl for this base.

      warning (0, G_ ("\t%s%s base %s"),
               a.string (),
               (virt ? " virtual" : ""),
               IDENTIFIER_POINTER (DECL_NAME (base_decl)));

      // Find the corresponding graph node.
      //
      node* base_node (unit_->find (base));
      assert (base_node != 0);
      unit_->new_edge<inherits> (
        *class_node, dynamic_cast<class_&> (*base_node), a, virt);
    }

    // Traverse data members.
    //
    for (tree d (TYPE_FIELDS (c)); d != NULL_TREE ; d = TREE_CHAIN (d))
    {
      //if (DECL_ARTIFICIAL (field))
      //  continue;

      // if (TREE_CODE (field) == TYPE_DECL && TREE_TYPE (field) == c)
      //   continue;

      switch (TREE_CODE (d))
      {
      case FIELD_DECL:
        {
          if (!DECL_ARTIFICIAL (d))
          {
            tree t (TREE_TYPE (d));
            char const* name (IDENTIFIER_POINTER (DECL_NAME (d)));

            path file (DECL_SOURCE_FILE (d));
            size_t line (DECL_SOURCE_LINE (d));
            size_t clmn (DECL_SOURCE_COLUMN (d));

            string type_name (emit_type_name (t));

            access a (decl_access (d));

            type& type_node (emit_type (t, file, line, clmn));
            data_member& member_node (
              unit_->new_node<data_member> (file, line, clmn));

            unit_->new_edge<belongs> (member_node, type_node);
            unit_->new_edge<names> (*class_node, member_node, name, a);

            warning (0, G_ ("\t%s data member %s (%p) %s in %s:%i"),
                     a.string (),
                     type_name.c_str (),
                     &type_node,
                     name,
                     file.string ().c_str (),
                     line);

            break;
          }
        }
      default:
        {
          /*
          tree name = DECL_NAME (d);

          if (name != NULL_TREE)
          {
            warning (0, G_ ("\tsome declaration %s in %s:%i"),
                     IDENTIFIER_POINTER (name),
                     DECL_SOURCE_FILE (d),
                     DECL_SOURCE_LINE (d));
          }
          else
          {
            warning (0, G_ ("\tsome unnamed declaration in %s:%i"),
                     DECL_SOURCE_FILE (d),
                     DECL_SOURCE_LINE (d));
          }
          */
          break;
        }
      }
    }

    return *class_node;
  }

  // Create new or find existing semantic graph type.
  //
  type&
  emit_type (tree t,
             path const& file,
             size_t line,
             size_t clmn)
  {
    node* n (unit_->find (TYPE_MAIN_VARIANT (t)));

    type& r (n != 0
             ? dynamic_cast<type&> (*n)
             : create_type (t, file, line, clmn));

    if (n != 0)
      warning (0, G_ ("emit_type: found node %p for type %p"),
               &r,
               TYPE_MAIN_VARIANT (t));

    if (cp_type_quals (t) == TYPE_UNQUALIFIED)
      return r;

    // See if this type already has this variant.
    //
    bool qc (CP_TYPE_CONST_P (t));
    bool qv (CP_TYPE_VOLATILE_P (t));
    bool qr (CP_TYPE_RESTRICT_P (t));

    for (type::qualified_iterator i (r.qualified_begin ());
         i != r.qualified_end (); ++i)
    {
      qualifier& q (i->qualifier ());

      if (q.const_ () == qc && q.volatile_ () == qv && q.restrict_ () == qr)
      {
        warning (0, G_ ("emit_type: found qualifier variant %p"), &q);
        return q;
      }
    }

    // No such variant yet. Create a new one.
    //
    qualifier& q (unit_->new_node<qualifier> (file, line, clmn, qc, qv, qr));
    unit_->new_edge<qualifies> (q, r);
    return q;
  }


  type&
  create_type (tree t,
               path const& file,
               size_t line,
               size_t clmn)
  {
    type* r (0);

    switch (TREE_CODE (t))
    {
      //
      // User-defined types.
      //
    case RECORD_TYPE:
      {
        tree ti (TYPE_TEMPLATE_INFO (t));

        if (ti == NULL_TREE)
        {
          // Ordinary class. There are two situations which can lead
          // here. First is when we have an anonymous class that is
          // part of the declaration, for example:
          //
          // typedef const struct {...} s;
          //
          // The second situation is a named class definition which
          // we haven't parsed yet. In this case we are going to
          // create a "stub" class node which will be processed and
          // filled in later.
          //

          t = TYPE_MAIN_VARIANT (t);
          tree d (TYPE_NAME (t));

          warning (0, G_ ("start anon/stub class declaration in %s:%d"),
                   file.string ().c_str (),
                   line);

          if (d == NULL_TREE || ANON_AGGRNAME_P (DECL_NAME (d)))
          {
            r = &emit_class (t, file, line, clmn);
          }
          else
          {
            // Use the "defining" declaration's file, line, and column
            // information to create the stub.
            //
            r = &emit_class (t,
                             path (DECL_SOURCE_FILE (d)),
                             DECL_SOURCE_LINE (d),
                             DECL_SOURCE_COLUMN (d),
                             true);
          }

          warning (0, G_ ("end anon/stub class declaration (%p) in %s:%d"),
                   r,
                   file.string ().c_str (),
                   line);
        }
        else
        {
          // Template instantiation.
          //
          /*
          tree decl (TI_TEMPLATE (ti)); // DECL_TEMPLATE
          string id (IDENTIFIER_POINTER (DECL_NAME (decl)));

          id += '<';

          tree args (INNERMOST_TEMPLATE_ARGS (TI_ARGS (ti)));

          int n (TREE_VEC_LENGTH (args));

          for (size_t i (0), n (TREE_VEC_LENGTH (args)); i < n ; ++i)
          {
            tree a (TREE_VEC_ELT (args, i));

            if (i != 0)
              id += ", ";

            // Assume type-only arguments.
            //
            id += emit_type (a);
          }

          id += '>';

          r = id + r;
          */
        }

        break;
      }

      /*

    case UNION_TYPE:
      {
        break;
      }
    case ENUMERAL_TYPE:
      {
        break;
      }

      */

      //
      // Derived types.
      //

    case ARRAY_TYPE:
      {
        unsigned long long size (0);

        if (tree index = TYPE_DOMAIN (t))
        {
          tree max (TYPE_MAX_VALUE (index));

          if (TREE_CODE (max) == INTEGER_CST)
          {
            HOST_WIDE_INT hwl (TREE_INT_CST_LOW (max));
            HOST_WIDE_INT hwh (TREE_INT_CST_HIGH (max));

            // The docs say that TYPE_DOMAIN will be NULL if the
            // array doesn't specify bounds. In reality, both
            // low and high parts are set to HOST_WIDE_INT_MAX.
            //
            if (hwl != ~HOST_WIDE_INT (0) && hwh != ~HOST_WIDE_INT (0))
            {
              unsigned long long l (hwl);
              unsigned long long h (hwh);
              unsigned short width (HOST_BITS_PER_WIDE_INT);

              size = (h << width) + l + 1;
            }
          }
          else
          {
            error (G_ ("%s:%d: non-integer array index %s"),
                   file.string ().c_str (),
                   line,
                   tree_code_name[TREE_CODE (max)]);
          }
        }

        type& bt (emit_type (TREE_TYPE (t), file, line, clmn));
        array& a (unit_->new_node<array> (file, line, clmn, size));
        unit_->new_edge<contains> (a, bt);
        unit_->insert (TYPE_MAIN_VARIANT (t), a);
        r = &a;
        break;
      }
    case REFERENCE_TYPE:
      {
        type& bt (emit_type (TREE_TYPE (t), file, line, clmn));
        reference& ref (unit_->new_node<reference> (file, line, clmn));
        unit_->new_edge<references> (ref, bt);
        unit_->insert (TYPE_MAIN_VARIANT (t), ref);
        r = &ref;
        break;
      }
    case POINTER_TYPE:
      {
        if (!TYPE_PTRMEM_P (t))
        {
          type& bt (emit_type (TREE_TYPE (t), file, line, clmn));
          pointer& p (unit_->new_node<pointer> (file, line, clmn));
          unit_->new_edge<points> (p, bt);
          unit_->insert (TYPE_MAIN_VARIANT (t), p);
          r = &p;
        }

        break;
      }
    default:
      {
        error (G_ ("%s:%d: unexpected type %s"),
               file.string ().c_str (),
               line,
               tree_code_name[TREE_CODE (t)]);
        break;
      }
    }

    return *r;
  }

  string
  emit_type_name (tree type, bool direct = true)
  {
    {
      warning (0, G_ ("type object %p; main %p"),
               type,
               TYPE_MAIN_VARIANT (type));

      for (tree v (TYPE_MAIN_VARIANT (type)); v != 0; v = TYPE_NEXT_VARIANT (v))
        warning (0, G_ ("\t variant %p"), v);
    }

    // First see if there is a "direct" name for this type.
    //
    if (direct)
    {
      if (tree decl = TYPE_NAME (type))
      {
        tree t (TREE_TYPE (decl));

        if (same_type_p (type, t))
          return IDENTIFIER_POINTER (DECL_NAME (decl));
      }
    }

    string r;

    if (CP_TYPE_CONST_P (type))
      r += " const";

    if (CP_TYPE_VOLATILE_P (type))
      r += " volatile";

    if (CP_TYPE_RESTRICT_P (type))
      r += " __restrict";

    switch (TREE_CODE (type))
    {
      //
      // User-defined types.
      //

    case RECORD_TYPE:
      {
        tree ti (TYPE_TEMPLATE_INFO (type));

        if (ti == NULL_TREE)
        {
          type = TYPE_MAIN_VARIANT (type);

          // Ordinary class.
          //
          tree decl (TYPE_NAME (type));
          r = IDENTIFIER_POINTER (DECL_NAME (decl)) + r;
        }
        else
        {
          // Template instantiation.
          //
          tree decl (TI_TEMPLATE (ti)); // DECL_TEMPLATE
          string id (IDENTIFIER_POINTER (DECL_NAME (decl)));

          id += '<';

          tree args (INNERMOST_TEMPLATE_ARGS (TI_ARGS (ti)));

          int n (TREE_VEC_LENGTH (args));

          for (size_t i (0), n (TREE_VEC_LENGTH (args)); i < n ; ++i)
          {
            tree a (TREE_VEC_ELT (args, i));

            if (i != 0)
              id += ", ";

            // Assume type-only arguments.
            //
            id += emit_type_name (a);
          }

          id += '>';

          r = id + r;
        }

        break;
      }

      /*
    case UNION_TYPE:
      {
        break;
      }
    case ENUMERAL_TYPE:
      {
        break;
      }
      */

      //
      // Derived types.
      //

    case ARRAY_TYPE:
      {
        unsigned long long size (0);

        if (tree index = TYPE_DOMAIN (type))
        {
          tree max (TYPE_MAX_VALUE (index));

          if (TREE_CODE (max) == INTEGER_CST)
          {
            HOST_WIDE_INT hwl (TREE_INT_CST_LOW (max));
            HOST_WIDE_INT hwh (TREE_INT_CST_HIGH (max));

            if (hwl != ~HOST_WIDE_INT (0) && hwh != ~HOST_WIDE_INT (0))
            {
              unsigned long long l (hwl);
              unsigned long long h (hwh);
              unsigned short width (HOST_BITS_PER_WIDE_INT);

              size = (h << width) + l + 1;
            }
          }
          else
          {
            error (G_ ("non-integer array index %s"),
                   tree_code_name[TREE_CODE (type)]);
          }
        }

        tree t (TREE_TYPE (type));

        if (size != 0)
        {
          ostringstream ostr;
          ostr << size;
          r = emit_type_name (t) + "[" + ostr.str () + "]" + r;
        }
        else
          r = emit_type_name (t) + "[]" + r;

        break;
      }
    case REFERENCE_TYPE:
      {
        tree t (TREE_TYPE (type));
        r = emit_type_name (t) + "&" + r;
        break;
      }
    case POINTER_TYPE:
      {
        if (!TYPE_PTRMEM_P (type))
        {
          tree t (TREE_TYPE (type));
          r = emit_type_name (t) + "*" + r;
        }

        break;
      }

      //
      // Fundamental types.
      //

    case VOID_TYPE:
    case REAL_TYPE:
    case BOOLEAN_TYPE:
    case INTEGER_TYPE:
      {
        type = TYPE_MAIN_VARIANT (type);
        tree decl (TYPE_NAME (type));
        r = IDENTIFIER_POINTER (DECL_NAME (decl)) + r;
        break;
      }
    default:
      {
        r = "<unsupported>";
        break;
      }
    }

    return r;
  }


  // Return declaration's fully-qualified scope name (e.g., ::foo::bar).
  //
  string
  fq_scope (tree decl)
  {
    string s, tmp;

    for (tree scope (CP_DECL_CONTEXT (decl));
         scope != global_namespace;
         scope = CP_DECL_CONTEXT (scope))
    {
      tree n = DECL_NAME (scope);

      tmp = "::";
      tmp += (n != NULL_TREE ? IDENTIFIER_POINTER (n) : "");
      tmp += s;
      s.swap (tmp);
    }

    return s;
  }

  // Return access for decl.
  //
  access
  decl_access (tree decl)
  {
    if (TREE_PRIVATE (decl))
      return access::private_;

    if (TREE_PROTECTED (decl))
      return access::protected_;

    return access::public_;
  }

  //
  //
  template <typename T>
  void
  define_fund (tree t)
  {
    t = TYPE_MAIN_VARIANT (t);
    char const* name (IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (t))));

    T& node (unit_->new_fund_node<T> ());
    unit_->new_edge<defines> (*scope_, node, name);
    unit_->insert (t, node);
  }

private:
  unit* unit_;
  scope* scope_;

  struct location_comparator
  {
    bool operator() (tree x, tree y) const
    {
      location_t xloc (DECL_SOURCE_LOCATION (x));
      location_t yloc (DECL_SOURCE_LOCATION (y));

      if (xloc != yloc)
        return xloc < yloc;

      size_t xl (LOCATION_LINE (xloc));
      size_t yl (LOCATION_LINE (yloc));

      if (xl != yl)
        return xl < yl;

      size_t xc (LOCATION_COLUMN (xloc));
      size_t yc (LOCATION_COLUMN (yloc));

      if (xc != yc)
        return xc < yc;

      return false;
    }
  };

  typedef std::multiset<tree, location_comparator> decl_set;
  decl_set decls_;

  typedef std::map<string, fund_type*> fund_type_map;
  fund_type_map fund_types_;
};

extern "C" void
gate_callback (void* gcc_data, void* user_data)
{
  warning (0, G_ ("main file is %s"), main_input_filename);

  if (!errorcount && !sorrycount)
  {
    parser p;
    auto_ptr<unit> u (p.parse (global_namespace, path (main_input_filename)));
  }

  exit (0);
}

extern "C" int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  warning (0, G_ ("starting plugin %s"), plugin_info->base_name);

  // Disable assembly output.
  //
  asm_file_name = HOST_BIT_BUCKET;

  register_callback (plugin_info->base_name,
                     PLUGIN_OVERRIDE_GATE,
                     &gate_callback,
                     NULL);

  return 0;
}
