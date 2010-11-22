// file      : odb/pragma.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/pragma.hxx>

using namespace std;

// Token spelling. See cpplib.h for details.
//
#define OP(e, s) s ,
#define TK(e, s) #e ,
static char const* token_spelling[N_TTYPES] = { TTYPE_TABLE };
#undef OP
#undef TK

// Lists of pragmas.
//
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
  bool first (true);

  if (tt == CPP_SCOPE)
  {
    name += "::";
    scope = global_namespace;
    first = false;
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
    {
      if (last)
      {
        error ((is_type
                ? "unable to resolve type name %qs in db pragma %qs"
                : "unable to resolve name %qs in db pragma %qs"),
               name.c_str (), prag.c_str ());
      }
      else
      {
        error ("unable to resolve name %qs in db pragma %qs",
               name.c_str (), prag.c_str ());
      }

      return 0;
    }

    scope = decl;

    if (last)
      break;

    first = false;

    if (TREE_CODE (scope) == TYPE_DECL)
      scope = TREE_TYPE (scope);

    name += "::";
    tt = pragma_lex (&t);
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

bool
check_decl_type (tree d, string const& name, string const& p, location_t l)
{
  int tc (TREE_CODE (d));
  char const* pc (p.c_str ());

  if (p == "member" ||
      p == "id" ||
      p == "auto" ||
      p == "column" ||
      p == "value_column" ||
      p == "index_column" ||
      p == "key_column" ||
      p == "id_column" ||
      p == "inverse" ||
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
           p == "pointer")
  {
    if (tc != RECORD_TYPE)
    {
      error_at (l, "name %qs in db pragma %qs does not refer to a class",
                name.c_str (), pc);
      return false;
    }
  }
  else if (p == "table")
  {
    // Table can be used for both members (container) and types.
    //
    if (tc != FIELD_DECL && tc != RECORD_TYPE)
    {
      error_at (l, "name %qs in db pragma %qs does not refer to a class "
                "or data member", name.c_str (), pc);
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
  else if (p == "type" ||
           p == "value_type" ||
           p == "index_type" ||
           p == "key_type")
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
handle_pragma (cpp_reader* reader,
               string const& p,
               tree decl,
               string const& decl_name)
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
  else if (p == "pointer")
  {
    // pointer (qname)
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

    size_t pb (0);

    for (tt = pragma_lex (&t);
         tt != CPP_EOF && (tt != CPP_CLOSE_PAREN || pb != 0);
         tt = pragma_lex (&t))
    {
      if (tt == CPP_OPEN_PAREN)
        pb++;
      else if (tt == CPP_CLOSE_PAREN)
        pb--;

      // @@ Need to handle literals, at least integer.
      //
      switch (tt)
      {
      case CPP_LESS:
        {
          val += "< ";
          break;
        }
      case CPP_GREATER:
        {
          val += " >";
          break;
        }
      case CPP_COMMA:
        {
          val += ", ";
          break;
        }
      case CPP_NAME:
        {
          val += IDENTIFIER_POINTER (t);
          break;
        }
      default:
        {
          if (tt <= CPP_LAST_PUNCTUATOR)
            val += token_spelling[tt];
          else
          {
            error ("unexpected token %qs in db pragma %qs",
                   token_spelling[tt],
                   pc);
            return;
          }
          break;
        }
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error ("%qs expected at the end of db pragma %qs", ")", pc);
      return;
    }

    if (val.empty ())
    {
      error ("expected pointer name in db pragma %qs", pc);
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
  else if (p == "column" ||
           p == "value_column" ||
           p == "index_column" ||
           p == "key_column" ||
           p == "id_column")
  {
    // column ("<name>")
    // value_column ("<name>")
    // index_column ("<name>")
    // key_column ("<name>")
    // id_column ("<name>")
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
  else if (p == "type" ||
           p == "value_type" ||
           p == "index_type" ||
           p == "key_type")
  {
    // type ("<type>")
    // value_type ("<type>")
    // index_type ("<type>")
    // key_type ("<type>")
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
  else if (p == "inverse")
  {
    // inverse (name)
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

    if (tt != CPP_NAME)
    {
      error ("member name expected in db pragma %qs", pc);
      return;
    }

    val = IDENTIFIER_POINTER (t);

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
    handle_pragma (reader, IDENTIFIER_POINTER (t), decl, decl_name);
  }
  else if (tt != CPP_EOF)
    error ("unexpected text after %qs in db pragma", p.c_str ());
}

static void
handle_pragma_qualifier (cpp_reader* reader, string const& p)
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
           p == "value_column" ||
           p == "index_column" ||
           p == "key_column" ||
           p == "id_column" ||
           p == "type" ||
           p == "value_type" ||
           p == "index_type" ||
           p == "key_type" ||
           p == "table" ||
           p == "inverse" ||
           p == "transient")
  {
    handle_pragma (reader, p, 0, "");
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
    handle_pragma (reader, IDENTIFIER_POINTER (t), decl, decl_name);
  }
  else if (tt != CPP_EOF)
    error ("unexpected text after %qs in db pragma", p.c_str ());
}

extern "C" void
handle_pragma_db_object (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "object");
}

extern "C" void
handle_pragma_db_value (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "value");
}

extern "C" void
handle_pragma_db_member (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "member");
}

extern "C" void
handle_pragma_db_id (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "id");
}

extern "C" void
handle_pragma_db_auto (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "auto");
}

extern "C" void
handle_pragma_db_column (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "column");
}

extern "C" void
handle_pragma_db_vcolumn (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "value_column");
}

extern "C" void
handle_pragma_db_icolumn (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "index_column");
}

extern "C" void
handle_pragma_db_kcolumn (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "key_column");
}

extern "C" void
handle_pragma_db_idcolumn (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "id_column");
}

extern "C" void
handle_pragma_db_type (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "type");
}

extern "C" void
handle_pragma_db_vtype (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "value_type");
}

extern "C" void
handle_pragma_db_itype (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "index_type");
}

extern "C" void
handle_pragma_db_ktype (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "key_type");
}

extern "C" void
handle_pragma_db_table (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "table");
}

extern "C" void
handle_pragma_db_inverse (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "inverse");
}

extern "C" void
handle_pragma_db_transient (cpp_reader* reader)
{
  handle_pragma_qualifier (reader, "transient");
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
  c_register_pragma_with_expansion ("db", "value_column", handle_pragma_db_vcolumn);
  c_register_pragma_with_expansion ("db", "index_column", handle_pragma_db_icolumn);
  c_register_pragma_with_expansion ("db", "key_column", handle_pragma_db_kcolumn);
  c_register_pragma_with_expansion ("db", "id_column", handle_pragma_db_idcolumn);
  c_register_pragma_with_expansion ("db", "type", handle_pragma_db_type);
  c_register_pragma_with_expansion ("db", "value_type", handle_pragma_db_vtype);
  c_register_pragma_with_expansion ("db", "index_type", handle_pragma_db_itype);
  c_register_pragma_with_expansion ("db", "key_type", handle_pragma_db_ktype);
  c_register_pragma_with_expansion ("db", "table", handle_pragma_db_table);
  c_register_pragma_with_expansion ("db", "inverse", handle_pragma_db_inverse);
  c_register_pragma_with_expansion ("db", "transient", handle_pragma_db_transient);
}
