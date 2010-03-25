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

          break;
        }
      case TEMPLATE_DECL:
        {
          if (DECL_CLASS_TEMPLATE_P (decl))
            decls_.insert (decl);
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
          emit_type_decl (decl);
          break;
        }
      case TEMPLATE_DECL:
        {
          emit_template_decl (decl);
          break;
        }
      }
    }
  }

  // Emit a type declaration. This is either a class definition/declaration
  // or a typedef.
  //
  void
  emit_type_decl (tree decl)
  {
    tree t (TREE_TYPE (decl));
    int tc (TREE_CODE (t));

    tree decl_name (DECL_NAME (decl));
    char const* name (IDENTIFIER_POINTER (decl_name));


    if (DECL_ARTIFICIAL (decl) &&
        (tc == RECORD_TYPE || tc == UNION_TYPE || tc == ENUMERAL_TYPE))
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
          // case will be covere by emit_type().
          //
          return;
        }
      }

      path file (DECL_SOURCE_FILE (decl));
      size_t line (DECL_SOURCE_LINE (decl));
      size_t clmn (DECL_SOURCE_COLUMN (decl));

      warning (0, G_ ("start %s declaration %s in %s:%d"),
               tree_code_name[tc],
               name,
               DECL_SOURCE_FILE (decl),
               DECL_SOURCE_LINE (decl));

      type* node (0);

      switch (tc)
      {
      case RECORD_TYPE:
        {
          node = &emit_class<class_> (t, file, line, clmn);
          break;
        }
      case UNION_TYPE:
        {
          node = &emit_union<union_> (t, file, line, clmn);
          break;
        }
      case ENUMERAL_TYPE:
        {
          node = &emit_enum (t, file, line, clmn);
          break;
        }
      }

      if (COMPLETE_TYPE_P (t))
        unit_->new_edge<defines> (*scope_, *node, name);
      else
        unit_->new_edge<declares> (*scope_, *node, name);

      warning (0, G_ ("end %s declaration %s (%p) in %s:%d"),
               tree_code_name[tc],
               name,
               node,
               DECL_SOURCE_FILE (decl),
               DECL_SOURCE_LINE (decl));
    }
    else
    {
      // Normal typedef. We need to detect and ignore the anonymous
      // class typedef case described above since we already used
      // this name to define the class.
      //
      int tc (TREE_CODE (t));

      if ((tc == RECORD_TYPE || tc == UNION_TYPE || tc == ENUMERAL_TYPE) &&
          TYPE_NAME (TYPE_MAIN_VARIANT (t)) == decl)
        return;

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
  }

  // Emit a template declaration.
  //
  void
  emit_template_decl (tree decl)
  {
    // Currently we only handle class/union templates.
    //
    tree t (TREE_TYPE (DECL_TEMPLATE_RESULT (decl)));
    int tc (TREE_CODE (t));

    warning (0, G_ ("%s template (%p) %s (%p) %s"),
             tree_code_name[tc],
             decl,
             IDENTIFIER_POINTER (DECL_NAME (decl)),
             t,
             tree_code_name[TREE_CODE (t)]);

    warning (0, G_ ("specialization:"));

    for (tree s (DECL_TEMPLATE_SPECIALIZATIONS (decl));
         s != NULL_TREE; s = TREE_CHAIN (s))
    {
      tree t (TREE_TYPE (s));
      tree d (TYPE_NAME (t));

      warning (0, G_ ("\tspecialization %p at %s:%d"),
               t,
               DECL_SOURCE_FILE (d),
               DECL_SOURCE_LINE (d));
    }

    warning (0, G_ ("instantiations:"));

    for (tree i (DECL_TEMPLATE_INSTANTIATIONS (decl));
         i != NULL_TREE; i = TREE_CHAIN (i))
    {
      tree t (TREE_VALUE (i));
      tree d (TYPE_NAME (t));

              warning (0, G_ ("\tinstantiation %p at %s:%d"),
                       t,
                       DECL_SOURCE_FILE (d),
                       DECL_SOURCE_LINE (d));
    }

    char const* name (IDENTIFIER_POINTER (DECL_NAME (decl)));

    warning (0, G_ ("start %s template %s in %s:%d"),
             tree_code_name[tc],
             name,
             DECL_SOURCE_FILE (decl),
             DECL_SOURCE_LINE (decl));

    type_template* t_node (0);

    if (tc == RECORD_TYPE)
      t_node = &emit_class_template (decl);
    else
      t_node = &emit_union_template (decl);

    if (COMPLETE_TYPE_P (t))
      unit_->new_edge<defines> (*scope_, *t_node, name);
    else
      unit_->new_edge<declares> (*scope_, *t_node, name);

    warning (0, G_ ("end %s template %s (%p) in %s:%d"),
             tree_code_name[tc],
             name,
             t_node,
             DECL_SOURCE_FILE (decl),
             DECL_SOURCE_LINE (decl));
  }

  class_template&
  emit_class_template (tree t, bool stub = false)
  {
    // See if there is a stub already for this template.
    //
    class_template* ct_node (0);

    if (node* n = unit_->find (t))
    {
      ct_node = &dynamic_cast<class_template&> (*n);
    }
    else
    {
      path f (DECL_SOURCE_FILE (t));
      size_t l (DECL_SOURCE_LINE (t));
      size_t c (DECL_SOURCE_COLUMN (t));

      ct_node = &unit_->new_node<class_template> (f, l, c);
      unit_->insert (t, *ct_node);
    }

    tree c (TREE_TYPE (DECL_TEMPLATE_RESULT (t)));

    if (stub || !COMPLETE_TYPE_P (c))
      return *ct_node;

    // Collect member declarations so that we can traverse them in
    // the source code order. For now we are only interested in
    // nested class template declarations.
    //
    decl_set decls;

    for (tree d (TYPE_FIELDS (c)); d != NULL_TREE ; d = TREE_CHAIN (d))
    {
      switch (TREE_CODE (d))
      {
      case TEMPLATE_DECL:
        {
          if (DECL_CLASS_TEMPLATE_P (d))
            decls.insert (d);
          break;
        }
      }
    }

    scope* prev_scope (scope_);
    scope_ = ct_node;

    for (decl_set::const_iterator i (decls.begin ()), e (decls.end ());
         i != e; ++i)
    {
      tree d (*i);

      switch (TREE_CODE (d))
      {
      case TEMPLATE_DECL:
        {
          emit_template_decl (d);
          break;
        }
      }
    }

    scope_ = prev_scope;
    return *ct_node;
  }

  union_template&
  emit_union_template (tree t, bool stub = false)
  {
    // See if there is a stub already for this template.
    //
    union_template* ut_node (0);

    if (node* n = unit_->find (t))
    {
      ut_node = &dynamic_cast<union_template&> (*n);
    }
    else
    {
      path f (DECL_SOURCE_FILE (t));
      size_t l (DECL_SOURCE_LINE (t));
      size_t c (DECL_SOURCE_COLUMN (t));

      ut_node = &unit_->new_node<union_template> (f, l, c);
      unit_->insert (t, *ut_node);
    }

    tree u (TREE_TYPE (DECL_TEMPLATE_RESULT (t)));

    if (stub || !COMPLETE_TYPE_P (u))
      return *ut_node;

    // Collect member declarations so that we can traverse them in
    // the source code order. For now we are only interested in
    // nested class template declarations.
    //
    decl_set decls;

    for (tree d (TYPE_FIELDS (u)); d != NULL_TREE ; d = TREE_CHAIN (d))
    {
      switch (TREE_CODE (d))
      {
      case TEMPLATE_DECL:
        {
          if (DECL_CLASS_TEMPLATE_P (d))
            decls.insert (d);
          break;
        }
      }
    }

    scope* prev_scope (scope_);
    scope_ = ut_node;

    for (decl_set::const_iterator i (decls.begin ()), e (decls.end ());
         i != e; ++i)
    {
      tree d (*i);

      switch (TREE_CODE (d))
      {
      case TEMPLATE_DECL:
        {
          emit_template_decl (d);
          break;
        }
      }
    }

    scope_ = prev_scope;
    return *ut_node;
  }

  template <typename T>
  T&
  emit_class (tree c,
              path const& file,
              size_t line,
              size_t clmn,
              bool stub = false)
  {
    c = TYPE_MAIN_VARIANT (c);

    // See if there is a stub already for this type.
    //
    T* c_node (0);

    if (node* n = unit_->find (c))
    {
      c_node = &dynamic_cast<T&> (*n);
    }
    else
    {
      c_node = &unit_->new_node<T> (file, line, clmn);
      unit_->insert (c, *c_node);
    }

    if (stub || !COMPLETE_TYPE_P (c))
      return *c_node;

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
      tree base (TYPE_MAIN_VARIANT (BINFO_TYPE (bi)));

      tree base_decl (TYPE_NAME (base)); // Typedef decl for this base.

      // Find the corresponding graph node. If we cannot find one then
      // the base is a template instantiation since an ordinary class
      // has to be defined (complete) in order to be a base.
      //
      class_* b_node (0);
      string name;

      if (node* n = unit_->find (base))
      {
        b_node = &dynamic_cast<class_&> (*n);
        name = IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (base)));
      }
      else
      {
        b_node = &dynamic_cast<class_&> (emit_type (base, file, line, clmn));
        name = emit_type_name (base);
      }

      unit_->new_edge<inherits> (*c_node, *b_node, a, virt);

      warning (0, G_ ("\t%s%s base %s (%p)"),
               a.string (),
               (virt ? " virtual" : ""),
               name.c_str (),
               static_cast<type*> (b_node));
    }

    // Collect member declarations so that we can traverse them in
    // the source code order.
    //
    decl_set decls;

    for (tree d (TYPE_FIELDS (c)); d != NULL_TREE ; d = TREE_CHAIN (d))
    {
      switch (TREE_CODE (d))
      {
      case TYPE_DECL:
        {
          if (!DECL_SELF_REFERENCE_P (d))
            decls.insert (d);
          break;
        }
      case TEMPLATE_DECL:
        {
          if (DECL_CLASS_TEMPLATE_P (d))
            decls.insert (d);
          break;
        }
      case FIELD_DECL:
        {
          if (!DECL_ARTIFICIAL (d))
            decls.insert (d);
          break;
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

    scope* prev_scope (scope_);
    scope_ = c_node;

    for (decl_set::const_iterator i (decls.begin ()), e (decls.end ());
         i != e; ++i)
    {
      tree d (*i);

      switch (TREE_CODE (d))
      {
      case TYPE_DECL:
        {
          emit_type_decl (d);
          break;
        }
      case TEMPLATE_DECL:
        {
          emit_template_decl (d);
          break;
        }
      case FIELD_DECL:
        {
          // If this is a bit-field then TREE_TYPE may be a modified type
          // with lesser precision. In this case, DECL_BIT_FIELD_TYPE
          // will be the type that was original specified. Use that type
          // for now. Furthermore, bitfields can be anonymous, which we
          // ignore.
          //
          //
          bool bf (DECL_C_BIT_FIELD (d));

          if (bf && DECL_NAME (d) == 0)
            break;

          tree t (bf ? DECL_BIT_FIELD_TYPE (d) : TREE_TYPE (d));

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
          unit_->new_edge<names> (*c_node, member_node, name, a);

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
    }

    scope_ = prev_scope;
    return *c_node;
  }

  template <typename T>
  T&
  emit_union (tree u,
              path const& file,
              size_t line,
              size_t clmn,
              bool stub = false)
  {
    u = TYPE_MAIN_VARIANT (u);

    // See if there is a stub already for this type.
    //
    T* u_node (0);

    if (node* n = unit_->find (u))
    {
      u_node = &dynamic_cast<T&> (*n);
    }
    else
    {
      u_node = &unit_->new_node<T> (file, line, clmn);
      unit_->insert (u, *u_node);
    }

    if (stub || !COMPLETE_TYPE_P (u))
      return *u_node;

    // Collect member declarations so that we can traverse them in
    // the source code order.
    //
    decl_set decls;

    for (tree d (TYPE_FIELDS (u)); d != NULL_TREE ; d = TREE_CHAIN (d))
    {
      switch (TREE_CODE (d))
      {
      case TYPE_DECL:
        {
          if (!DECL_SELF_REFERENCE_P (d))
            decls.insert (d);
          break;
        }
      case TEMPLATE_DECL:
        {
          if (DECL_CLASS_TEMPLATE_P (d))
            decls.insert (d);
          break;
        }
      case FIELD_DECL:
        {
          if (!DECL_ARTIFICIAL (d))
            decls.insert (d);
          break;
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

    scope* prev_scope (scope_);
    scope_ = u_node;

    for (decl_set::const_iterator i (decls.begin ()), e (decls.end ());
         i != e; ++i)
    {
      tree d (*i);

      switch (TREE_CODE (d))
      {
      case TYPE_DECL:
        {
          emit_type_decl (d);
          break;
        }
      case TEMPLATE_DECL:
        {
          emit_template_decl (d);
          break;
        }
      case FIELD_DECL:
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
          unit_->new_edge<names> (*u_node, member_node, name, a);

          warning (0, G_ ("\t%s union member %s (%p) %s in %s:%i"),
                   a.string (),
                   type_name.c_str (),
                   &type_node,
                   name,
                   file.string ().c_str (),
                   line);

          break;
        }
      }
    }

    scope_ = prev_scope;
    return *u_node;
  }

  enum_&
  emit_enum (tree e,
             path const& file,
             size_t line,
             size_t clmn,
             bool stub = false)
  {
    e = TYPE_MAIN_VARIANT (e);

    // See if there is a stub already for this type.
    //
    enum_* e_node (0);

    if (node* n = unit_->find (e))
    {
      e_node = &dynamic_cast<enum_&> (*n);
    }
    else
    {
      e_node = &unit_->new_node<enum_> (file, line, clmn);
      unit_->insert (e, *e_node);
    }

    if (stub || !COMPLETE_TYPE_P (e))
      return *e_node;

    // Traverse enumerators.
    //
    for (tree er (TYPE_VALUES (e)); er != NULL_TREE ; er = TREE_CHAIN (er))
    {
      char const* name (IDENTIFIER_POINTER (TREE_PURPOSE (er)));

      // There doesn't seem to be a way to get the proper position for
      // each enumerator.
      //
      enumerator& er_node = unit_->new_node<enumerator> (file, line, clmn);
      unit_->new_edge<enumerates> (*e_node, er_node);

      warning (0, G_ ("\tenumerator %s in %s:%d"),
               name,
               file.string ().c_str (),
               line);
    }

    return *e_node;
  }

  // Create new or find existing semantic graph type.
  //
  type&
  emit_type (tree t,
             path const& file,
             size_t line,
             size_t clmn)
  {
    {
      warning (0, G_ ("%s %p; main %p"),
               tree_code_name[TREE_CODE (t)],
               t,
               TYPE_MAIN_VARIANT (t));

      for (tree v (TYPE_MAIN_VARIANT (t)); v != 0; v = TYPE_NEXT_VARIANT (v))
        warning (0, G_ ("\t variant %p"), v);
    }

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
    int tc (TREE_CODE (t));

    switch (tc)
    {
      //
      // User-defined types.
      //
    case RECORD_TYPE:
    case UNION_TYPE:
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

          warning (0, G_ ("start anon/stub %s declaration in %s:%d"),
                   tree_code_name[tc],
                   file.string ().c_str (),
                   line);

          if (d == NULL_TREE || ANON_AGGRNAME_P (DECL_NAME (d)))
          {
            if (tc == RECORD_TYPE)
              r = &emit_class<class_> (t, file, line, clmn);
            else
              r = &emit_union<union_> (t, file, line, clmn);
          }
          else
          {
            // Use the "defining" declaration's file, line, and column
            // information to create the stub.
            //
            path f (DECL_SOURCE_FILE (d));
            size_t l (DECL_SOURCE_LINE (d));
            size_t c (DECL_SOURCE_COLUMN (d));

            if (tc == RECORD_TYPE)
              r = &emit_class<class_> (t, f, l, c, true);
            else
              r = &emit_union<union_> (t, f, l, c, true);
          }

          warning (0, G_ ("end anon/stub %s declaration (%p) in %s:%d"),
                   tree_code_name[tc],
                   r,
                   file.string ().c_str (),
                   line);
        }
        else
        {
          // Template instantiation.
          //
          t = TYPE_MAIN_VARIANT (t);
          tree decl (TI_TEMPLATE (ti)); // DECL_TEMPLATE

          // Get to the most general template declaration.
          //
          while (DECL_TEMPLATE_INFO (decl))
            decl = DECL_TI_TEMPLATE (decl);

          type_template* t_node (0);

          // Find the template node or create a stub if none exist.
          //
          if (node* n = unit_->find (decl))
            t_node = &dynamic_cast<type_template&> (*n);
          else
          {
            warning (0, G_ ("start stub %s template for (%p) in %s:%d"),
                     tree_code_name[tc],
                     decl,
                     file.string ().c_str (),
                     line);

            if (tc == RECORD_TYPE)
              t_node = &emit_class_template (decl, true);
            else
              t_node = &emit_union_template (decl, true);

            warning (0, G_ ("end stub %s template (%p) in %s:%d"),
                     tree_code_name[tc],
                     t_node,
                     file.string ().c_str (),
                     line);
          }

          warning (0, G_ ("start %s instantiation (%p) for template (%p) in %s:%d"),
                   tree_code_name[tc],
                   t,
                   t_node,
                   file.string ().c_str (),
                   line);

          type_instantiation* i_node (0);

          if (tc == RECORD_TYPE)
            i_node = &emit_class<class_instantiation> (t, file, line, clmn);
          else
            i_node = &emit_union<union_instantiation> (t, file, line, clmn);

          warning (0, G_ ("end %s instantiation (%p) in %s:%d"),
                   tree_code_name[tc],
                   static_cast<type*> (i_node),
                   file.string ().c_str (),
                   line);

          unit_->new_edge<instantiates> (*i_node, *t_node);
          r = i_node;
        }

        break;
      }
    case ENUMERAL_TYPE:
      {
        // The same logic as in the "ordinary class" case above
        // applies here.
        //

        t = TYPE_MAIN_VARIANT (t);
        tree d (TYPE_NAME (t));

        warning (0, G_ ("start anon/stub %s declaration in %s:%d"),
                 tree_code_name[tc],
                 file.string ().c_str (),
                 line);

        if (d == NULL_TREE || ANON_AGGRNAME_P (DECL_NAME (d)))
        {
          r = &emit_enum (t, file, line, clmn);
        }
        else
        {
          // Use the "defining" declaration's file, line, and column
          // information to create the stub.
          //
          path f (DECL_SOURCE_FILE (d));
          size_t l (DECL_SOURCE_LINE (d));
          size_t c (DECL_SOURCE_COLUMN (d));

          r = &emit_enum (t, f, l, c, true);
        }

        warning (0, G_ ("end anon/stub %s declaration (%p) in %s:%d"),
                 tree_code_name[tc],
                 r,
                 file.string ().c_str (),
                 line);

        break;
      }

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
        else
        {
          r = &unit_->new_node<unsupported_type> (
            file, line, clmn, "pointer_to_member_type");

          warning (0, G_ ("unsupported pointer_to_member_type (%p) in %s:%d"),
                   r,
                   file.string ().c_str (),
                   line);
        }
        break;
      }
    default:
      {
        r = &unit_->new_node<unsupported_type> (
          file, line, clmn, tree_code_name[tc]);

        warning (0, G_ ("unsupported %s (%p) in %s:%d"),
                 tree_code_name[tc],
                 r,
                 file.string ().c_str (),
                 line);
        break;
      }
    }

    return *r;
  }

  string
  emit_type_name (tree type, bool direct = true)
  {
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

    int tc (TREE_CODE (type));

    switch (tc)
    {
      //
      // User-defined types.
      //

    case RECORD_TYPE:
    case UNION_TYPE:
      {
        tree ti (TYPE_TEMPLATE_INFO (type));

        if (ti == NULL_TREE)
        {
          type = TYPE_MAIN_VARIANT (type);

          // Ordinary class. Some synthesized stuff (e.g., member-function-
          // pointer-struct) can be really anonymous so check that.
          //
          tree name (TYPE_NAME (type));
          r = (name ? IDENTIFIER_POINTER (DECL_NAME (name)) : "<anonymous>") + r;
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

    case ENUMERAL_TYPE:
      {
        type = TYPE_MAIN_VARIANT (type);
        tree decl (TYPE_NAME (type));
        r = IDENTIFIER_POINTER (DECL_NAME (decl)) + r;
        break;
      }

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
        else
          r = "<pointer_to_member_type>";

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
        r = "<" + string (tree_code_name[tc]) + ">";
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
