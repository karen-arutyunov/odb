// file      : odb/lookup.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/lookup.hxx>

using namespace std;

namespace lookup
{
  std::string
  parse_scoped_name (std::string& t, cpp_ttype& tt, cxx_lexer& lex)
  {
    string name;

    if (tt == CPP_SCOPE)
    {
      name += "::";
      tt = lex.next (t);
    }

    while (true)
    {
      // @@ We still need to handle fundamental types, e.g., unsigned int.
      //
      if (tt != CPP_NAME && tt != CPP_KEYWORD)
        throw invalid_name ();

      name += t;
      tt = lex.next (t);

      if (tt != CPP_SCOPE)
        break;

      name += "::";
      tt = lex.next (t);
    }

    return name;
  }

  tree
  resolve_scoped_name (string& t,
                       cpp_ttype& tt,
                       cpp_ttype& ptt,
                       cxx_lexer& lex,
                       tree scope,
                       string& name,
                       bool is_type,
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
      tt = lex.next (t);
    }

    while (true)
    {
      if (end_scope != 0)
        *end_scope = scope;

      // @@ We still need to handle fundamental types, e.g., unsigned int.
      //
      if (tt != CPP_NAME && tt != CPP_KEYWORD)
        throw invalid_name ();

      name += t;
      id = get_identifier (t.c_str ());
      ptt = tt;
      tt = lex.next (t);

      bool last (tt != CPP_SCOPE);
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

      ptt = tt;
      tt = lex.next (t);
    }

    // Get the actual type if this is a TYPE_DECL.
    //
    if (is_type)
    {
      if (TREE_CODE (scope) == TYPE_DECL)
        scope = TREE_TYPE (scope);

      scope = TYPE_MAIN_VARIANT (scope);
    }

    return scope;
  }
}
