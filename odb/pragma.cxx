// file      : odb/pragma.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

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
      error ("invalid name in db pragma %qs", prag.c_str ());
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
        error ((is_type
                ? "unable to resolve type name %qs in db pragma %qs"
                : "unable to resolve name %qs in db pragma %qs"),
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
    error ((is_type
            ? "unable to resolve type name %qs in db pragma %qs"
            : "unable to resolve name %qs in db pragma %qs"),
           name.c_str (), prag.c_str ());
    return 0;
  }

  // Get the actual type if this is a TYPE_DECL.
  //
  if (is_type)
  {
    if (TREE_CODE (decl) == TYPE_DECL)
      decl = TREE_TYPE (decl);

    decl = TYPE_MAIN_VARIANT (decl);
  }

  return decl;
}

bool
check_decl_type (tree d, string const& name, string const& p, location_t l)
{
  int tc (TREE_CODE (d));
  char const* pc (p.c_str ());

  if (p == "member" ||
      p == "id" ||
      p == "auto" ||
      p == "column" ||
      p == "transient")
  {
    if (tc != FIELD_DECL)
    {
      error_at (l, "name %qs in db pragma %qs does not refer to "
                "a data member", name.c_str (), pc);
      return false;
    }
  }
  else if (p == "object" ||
           p == "table")
  {
    if (tc != RECORD_TYPE)
    {
      error_at (l, "name %qs in db pragma %qs does not refer to a class",
                name.c_str (), pc);
      return false;
    }
  }
  else if (p == "value")
  {
    if (!TYPE_P (d))
    {
      error_at (l, "name %qs in db pragma %qs does not refer to a type",
                name.c_str (), pc);
      return false;
    }
  }
  else if (p == "type")
  {
    // Type can be used for both members and types.
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error_at (l, "name %qs in db pragma %qs does not refer to "
                "a type or data member", name.c_str (), pc);
      return false;
    }
  }
  else
  {
    error ("unknown db pragma %qs", pc);
    return false;
  }

  return true;
}

static void
handle_pragma (string const& p, tree decl, string const& decl_name)
{
  tree t;
  cpp_ttype tt;
  char const* pc (p.c_str ());

  string val;
  location_t loc (input_location);

  if (p == "table")
  {
    // table ("<name>")
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error ("%qs expected after db pragma %qs", "(", pc);
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error ("table name expected in db pragma %qs", pc);
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error ("%qs expected at the end of db pragma %qs", ")", pc);
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "id")
  {
    // id
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "auto")
  {
    // auto
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "column")
  {
    // column ([<identifier>,] "<name>")
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error ("%qs expected after db pragma %qs", "(", pc);
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error ("column name expected in db pragma %qs", pc);
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error ("%qs expected at the end of db pragma %qs", ")", pc);
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "type")
  {
    // type ("<type>")
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error ("%qs expected after db pragma %qs", "(", pc);
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error ("type name expected in db pragma %qs", pc);
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error ("%qs expected at the end of db pragma %qs", ")", pc);
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "transient")
  {
    // transient
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else
  {
    error ("unknown db pragma %qs", pc);
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
  if (tt == CPP_NAME)
  {
    handle_pragma (IDENTIFIER_POINTER (t), decl, decl_name);
  }
  else if (tt != CPP_EOF)
    error ("unexpected text after %qs in db pragma", p.c_str ());
}

static void
handle_pragma_qualifier (string const& p)
{
  tree t;
  cpp_ttype tt;
  char const* pc (p.c_str ());

  tree decl (0);
  string decl_name;
  location_t loc (input_location);

  // Pragma qualifiers.
  //
  if (p == "object")
  {
    // object [(<identifier>)]
    //

    tt = pragma_lex (&t);

    if (tt == CPP_OPEN_PAREN)
    {
      tt = pragma_lex (&t);

      if (tt == CPP_NAME || tt == CPP_SCOPE)
      {
        decl = parse_scoped_name (t, tt, decl_name, true, p);

        if (decl == 0)
          return;

        // Make sure we've got the correct declaration type.
        //
        if (!check_decl_type (decl, decl_name, p, loc))
          return;

        if (tt != CPP_CLOSE_PAREN)
        {
          error ("%qs expected at the end of db pragma %qs", ")", pc);
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error ("type name expected in db pragma %qs", pc);
        return;
      }
    }
  }
  else if (p == "value")
  {
    // value [(<identifier>)]
    //

    tt = pragma_lex (&t);

    if (tt == CPP_OPEN_PAREN)
    {
      tt = pragma_lex (&t);

      if (tt == CPP_NAME || tt == CPP_SCOPE)
      {
        decl = parse_scoped_name (t, tt, decl_name, true, p);

        if (decl == 0)
          return;

        // Make sure we've got the correct declaration type.
        //
        if (!check_decl_type (decl, decl_name, p, loc))
          return;

        if (tt != CPP_CLOSE_PAREN)
        {
          error ("%qs expected at the end of db pragma %qs", ")", pc);
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error ("type name expected in db pragma %qs", pc);
        return;
      }
    }
  }
  else if (p == "member")
  {
    // member [(<identifier>)]
    //

    tt = pragma_lex (&t);

    if (tt == CPP_OPEN_PAREN)
    {
      tt = pragma_lex (&t);

      if (tt == CPP_NAME || tt == CPP_SCOPE)
      {
        decl = parse_scoped_name (t, tt, decl_name, false, p);

        if (decl == 0)
          return;

        // Make sure we've got the correct declaration type.
        //
        if (!check_decl_type (decl, decl_name, p, loc))
          return;

        if (tt != CPP_CLOSE_PAREN)
        {
          error ("%qs expected at the end of db pragma %qs", ")", pc);
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error ("data member name expected in db pragma %qs", pc);
        return;
      }
    }
  }
  //
  // The member qualifier can be omitted so we need to also handle all
  // the member pragmas here.
  //
  else if (p == "id" ||
           p == "auto" ||
           p == "column" ||
           p == "type" ||
           p == "transient")
  {
    handle_pragma (p, 0, "");
    return;
  }
  else
  {
    error ("unknown db pragma %qs", pc);
    return;
  }

  // Record this pragma.
  //
  pragma prag (p, "", loc);

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
  if (tt == CPP_NAME)
  {
    handle_pragma (IDENTIFIER_POINTER (t), decl, decl_name);
  }
  else if (tt != CPP_EOF)
    error ("unexpected text after %qs in db pragma", p.c_str ());
}

extern "C" void
handle_pragma_db_object (cpp_reader*)
{
  handle_pragma_qualifier ("object");
}

extern "C" void
handle_pragma_db_value (cpp_reader*)
{
  handle_pragma_qualifier ("value");
}

extern "C" void
handle_pragma_db_member (cpp_reader*)
{
  handle_pragma_qualifier ("member");
}

extern "C" void
handle_pragma_db_id (cpp_reader*)
{
  handle_pragma_qualifier ("id");
}

extern "C" void
handle_pragma_db_auto (cpp_reader*)
{
  handle_pragma_qualifier ("auto");
}

extern "C" void
handle_pragma_db_column (cpp_reader*)
{
  handle_pragma_qualifier ("column");
}

extern "C" void
handle_pragma_db_type (cpp_reader*)
{
  handle_pragma_qualifier ("type");
}

extern "C" void
handle_pragma_db_transient (cpp_reader*)
{
  handle_pragma_qualifier ("transient");
}

extern "C" void
register_odb_pragmas (void*, void*)
{
  c_register_pragma_with_expansion ("db", "object", handle_pragma_db_object);
  c_register_pragma_with_expansion ("db", "value", handle_pragma_db_value);
  c_register_pragma_with_expansion ("db", "member", handle_pragma_db_member);
  c_register_pragma_with_expansion ("db", "id", handle_pragma_db_id);
  c_register_pragma_with_expansion ("db", "auto", handle_pragma_db_auto);
  c_register_pragma_with_expansion ("db", "column", handle_pragma_db_column);
  c_register_pragma_with_expansion ("db", "type", handle_pragma_db_type);
  c_register_pragma_with_expansion ("db", "transient", handle_pragma_db_transient);
}
