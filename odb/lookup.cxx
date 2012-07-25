// file      : odb/lookup.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/lookup.hxx>

using namespace std;

namespace lookup
{
  std::string
  parse_scoped_name (cxx_lexer& l, cpp_ttype& tt, string& tl, tree& tn)
  {
    string name;

    if (tt == CPP_SCOPE)
    {
      name += "::";
      tt = l.next (tl, &tn);
    }

    while (true)
    {
      // @@ We still need to handle fundamental types, e.g., unsigned int.
      //
      if (tt != CPP_NAME && tt != CPP_KEYWORD)
        throw invalid_name ();

      name += tl;
      tt = l.next (tl, &tn);

      if (tt != CPP_SCOPE)
        break;

      name += "::";
      tt = l.next (tl, &tn);
    }

    return name;
  }

  tree
  resolve_scoped_name (cxx_lexer& l,
                       cpp_ttype& tt,
                       string& tl,
                       tree& tn,
                       cpp_ttype& ptt,
                       tree scope,
                       string& name,
                       bool is_type,
                       bool trailing_scope,
                       tree* end_scope)
  {
    tree id;
    bool first (true);

    if (tt == CPP_SCOPE)
    {
      name += "::";
      scope = global_namespace;
      first = false;

      ptt = tt;
      tt = l.next (tl, &tn);
    }

    while (true)
    {
      if (end_scope != 0)
        *end_scope = scope;

      // @@ We still need to handle fundamental types, e.g., unsigned int.
      //
      if (tt != CPP_NAME && tt != CPP_KEYWORD)
        throw invalid_name ();

      name += tl;
      id = get_identifier (tl.c_str ());
      ptt = tt;
      tt = l.next (tl, &tn);

      bool last (true);
      if (tt == CPP_SCOPE)
      {
        // If trailing scope names are allowed, then we also need to
        // check what's after the scope.
        //
        if (trailing_scope)
        {
          ptt = tt;
          tt = l.next (tl, &tn);

          if (tt == CPP_NAME)
            last = false;
        }
        else
          last = false;
      }

      tree decl = lookup_qualified_name (scope, id, last && is_type, false);

      // If this is the first component in the name, then also search the
      // outer scopes.
      //
      if (decl == error_mark_node && first && scope != global_namespace)
      {
        do
        {
          scope = TYPE_P (scope)
            ? CP_TYPE_CONTEXT (scope)
            : CP_DECL_CONTEXT (scope);
          decl = lookup_qualified_name (scope, id, last && is_type, false);
        } while (decl == error_mark_node && scope != global_namespace);
      }

      if (decl == error_mark_node)
        throw unable_to_resolve (name, last);

      scope = decl;

      if (last)
        break;

      first = false;

      if (TREE_CODE (scope) == TYPE_DECL)
        scope = TREE_TYPE (scope);

      name += "::";

      if (!trailing_scope)
      {
        ptt = tt;
        tt = l.next (tl, &tn);
      }
    }

    return scope;
  }
}
