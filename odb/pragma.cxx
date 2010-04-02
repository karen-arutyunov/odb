// file      : odb/pragma.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/pragma.hxx>

using namespace std;

loc_pragmas loc_pragmas_;
decl_pragmas decl_pragmas_;

static tree
parse_scoped_name (tree& t,
                   cpp_ttype& tt,
                   string& name,
                   bool is_type,
                   string const& prag)
{
  tree scope, id;

  if (tt == CPP_SCOPE)
  {
    name += "::";
    scope = global_namespace;
    tt = pragma_lex (&t);
  }
  else
    scope = current_scope ();

  while (true)
  {
    if (tt != CPP_NAME)
    {
      error ("invalid name in odb pragma %qs", prag.c_str ());
      return 0;
    }

    id = t;
    name += IDENTIFIER_POINTER (t);
    tt = pragma_lex (&t);

    if (tt == CPP_SCOPE)
    {
      scope = lookup_qualified_name (scope, id, false, false);

      if (scope == error_mark_node)
      {
        error ("unable to resolve name %qs in odb pragma %qs",
               name.c_str (), prag.c_str ());
        return 0;
      }

      if (TREE_CODE (scope) == TYPE_DECL)
        scope = TREE_TYPE (scope);

      name += "::";
      tt = pragma_lex (&t);
    }
    else
      break;
  }

  tree decl (lookup_qualified_name (scope, id, is_type, false));

  if (decl == error_mark_node)
  {
    error ("unable to resolve name %qs in odb pragma %qs",
           name.c_str (), prag.c_str ());
    return 0;
  }

  // Get the actual type if this is a TYPE_DECL.
  //
  if (is_type && TREE_CODE (decl) == TYPE_DECL)
    decl = TREE_TYPE (decl);

  return decl;
}

bool
check_decl_type (tree d, string const& name, string const& p, location_t l)
{
  int tc (TREE_CODE (d));
  char const* pc (p.c_str ());

  if (p == "column")
  {
    if (tc != FIELD_DECL)
    {
      error_at (l, "name %qs in odb pragma %qs does not refer to "
                "a member variable", name.c_str (), pc);
      return false;
    }
  }
  else if (p == "table")
  {
    if (tc != RECORD_TYPE)
    {
      error_at (l, "name %qs in odb pragma %qs does not refer to a class",
                name.c_str (), pc);
      return false;
    }
  }
  else
  {
    error ("unknown odb pragma %qs", pc);
    return false;
  }

  return true;
}

static void
handle_pragma (string const& p)
{
  tree t;
  cpp_ttype tt;
  char const* pc (p.c_str ());

  string val;
  tree decl (0);
  location_t loc (input_location);

  if (p == "column")
  {
    // column ([<identifier>,] "<name>")
    //

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error ("%qs expected after odb pragma %qs", "(", pc);
      return;
    }

    tt = pragma_lex (&t);

    if (tt == CPP_NAME || tt == CPP_SCOPE)
    {
      string name;
      decl = parse_scoped_name (t, tt, name, false, p);

      if (decl == 0)
        return;

      // Make sure we've got the correct declaration type.
      //
      if (!check_decl_type (decl, name, p, loc))
        return;

      if (tt != CPP_COMMA)
      {
        error ("column name expected in odb pragma %qs", pc);
        return;
      }

      tt = pragma_lex (&t);
    }

    if (tt != CPP_STRING)
    {
      error ("column name expected in odb pragma %qs", pc);
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error ("%qs expected at the end of odb pragma %qs", ")", pc);
      return;
    }
  }
  else if (p == "table")
  {
    // table ([<identifier>,] "<name>")
    //

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error ("%qs expected after odb pragma %qs", "(", pc);
      return;
    }

    tt = pragma_lex (&t);

    if (tt == CPP_NAME || tt == CPP_SCOPE)
    {
      string name;
      decl = parse_scoped_name (t, tt, name, true, p);

      if (decl == 0)
        return;

      // Make sure we've got the correct declaration type.
      //
      if (!check_decl_type (decl, name, p, loc))
        return;

      if (tt != CPP_COMMA)
      {
        error ("table name expected in odb pragma %qs", pc);
        return;
      }

      tt = pragma_lex (&t);
    }

    if (tt != CPP_STRING)
    {
      error ("table name expected in odb pragma %qs", pc);
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error ("%qs expected at the end of odb pragma %qs", ")", pc);
      return;
    }
  }
  else
  {
    error ("unknown odb pragma %qs", pc);
    return;
  }

  // Record this pragma.
  //
  pragma prag (p, val, loc);

  if (decl)
    decl_pragmas_[decl].insert (prag);
  else
  {
    tree scope (current_scope ());

    if (!CLASS_TYPE_P (scope))
      scope = global_namespace;

    loc_pragmas_[scope].push_back (prag);
  }

  // See if there are any more pragmas.
  //
  tt = pragma_lex (&t);

  if (tt == CPP_NAME)
  {
    handle_pragma (IDENTIFIER_POINTER (t));
  }
  else if (tt != CPP_EOF)
    error ("unexpected text after %qs in odb pragma", p.c_str ());
}

extern "C" void
handle_pragma_odb_column (cpp_reader*)
{
  handle_pragma ("column");
}

extern "C" void
handle_pragma_odb_table (cpp_reader*)
{
  handle_pragma ("table");
}

extern "C" void
register_odb_pragmas (void*, void*)
{
  c_register_pragma_with_expansion ("odb", "column", handle_pragma_odb_column);
  c_register_pragma_with_expansion ("odb", "table", handle_pragma_odb_table);
}
