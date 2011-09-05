// file      : odb/pragma.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/error.hxx>
#include <odb/pragma.hxx>
#include <odb/cxx-lexer.hxx>

using namespace std;

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
      error () << "invalid name in db pragma '" << prag << "'" << endl;
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
        error () << "unable to resolve " << (is_type ? "type " : "") << "name "
                 << "'" << name << "' in db pragma '" << prag << "'" << endl;
      }
      else
      {
        error () << "unable to resolve name '" << name << "' in db pragma '"
                 << prag << "'" << endl;
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

  if (p == "member" ||
      p == "id" ||
      p == "auto" ||
      p == "column" ||
      p == "inverse" ||
      p == "transient")
  {
    if (tc != FIELD_DECL)
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a data member" << endl;
      return false;
    }
  }
  else if (p == "object" ||
           p == "view" ||
           p == "pointer" ||
           p == "abstract" ||
           p == "callback" ||
           p == "query")
  {
    if (tc != RECORD_TYPE)
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a class" << endl;
      return false;
    }
  }
  else if (p == "table")
  {
    // Table can be used for both members (container) and types (container
    // object, or view).
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "value")
  {
    if (!TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type" << endl;
      return false;
    }
  }
  else if (p == "id_type")
  {
    // Id type can only be used for types.
    //
    if (!TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type" << endl;
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
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "default")
  {
    // Default can be used for both members and types.
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "value_column" ||
           p == "index_column" ||
           p == "key_column" ||
           p == "id_column")
  {
    // Container columns can be used for both members (container) and
    // types (container).
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "options" ||
           p == "value_options" ||
           p == "index_options" ||
           p == "key_options" ||
           p == "id_options")
  {
    // Options can be used for both members and types.
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "null" ||
           p == "not_null" ||
           p == "value_null" ||
           p == "value_not_null")
  {
    // Null pragmas can be used for both members and types (values,
    // containers, and pointers).
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "unordered")
  {
    // Unordered can be used for both members (container) and
    // types (container).
    //
    if (tc != FIELD_DECL && !TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma '" << p << "' does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else
  {
    error () << "unknown db pragma '" << p << "'" << endl;
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

  string val;
  tree node (0);
  location_t loc (input_location);
  pragma::mode_type mode (pragma::override);

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
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "table name expected in db pragma '" << p << "'" << endl;
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
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
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    size_t pb (0);
    bool punc (false);

    for (tt = pragma_lex (&t);
         tt != CPP_EOF && (tt != CPP_CLOSE_PAREN || pb != 0);
         tt = pragma_lex (&t))
    {
      if (punc && tt > CPP_LAST_PUNCTUATOR)
        val += ' ';

      punc = false;

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
          punc = true;
          break;
        }
      default:
        {
          if (tt <= CPP_LAST_PUNCTUATOR)
            val += cxx_lexer::token_spelling[tt];
          else
          {
            error () << "unexpected token '" << cxx_lexer::token_spelling[tt]
                     << "' in db pragma '" << p << "'" << endl;
            return;
          }
          break;
        }
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
      return;
    }

    if (val.empty ())
    {
      error () << "expected pointer name in db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "abstract")
  {
    // abstract
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "callback")
  {
    // callback (name)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_NAME)
    {
      error () << "member function name expected in db pragma '" << p
               << "'" << endl;
      return;
    }

    val = IDENTIFIER_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "query")
  {
    // query ("statement")
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "query statement expected in db pragma '" << p << "'" << endl;
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
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
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "column name expected in db pragma '" << p << "'" << endl;
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "options" ||
           p == "value_options" ||
           p == "index_options" ||
           p == "key_options" ||
           p == "id_options")
  {
    // options (["<name>"])
    // value_options (["<name>"])
    // index_options (["<name>"])
    // key_options (["<name>"])
    // id_options (["<name>"])
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    // An empty options specifier signals options reset.
    //
    if (tt == CPP_STRING)
    {
      val = TREE_STRING_POINTER (t);
      tt = pragma_lex (&t);
    }
    // Empty options specifier signals options reset.
    //
    else if (tt != CPP_CLOSE_PAREN)
    {
      error () << "options string expected in db pragma '" << p << "'" << endl;
      return;
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
      return;
    }

    mode = pragma::accumulate;
    tt = pragma_lex (&t);
  }
  else if (p == "type" ||
           p == "id_type" ||
           p == "value_type" ||
           p == "index_type" ||
           p == "key_type")
  {
    // type ("<type>")
    // id_type ("<type>")
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
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "type name expected in db pragma '" << p << "'" << endl;
      return;
    }

    val = TREE_STRING_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "null" ||
           p == "not_null" ||
           p == "value_null" ||
           p == "value_not_null")
  {
    // null
    // not_null
    // value_null
    // value_not_null
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "default")
  {
    // default ()                  (<empty>)
    // default (null)              (n)
    // default (true|false)        (t|f)
    // default ([+|-]<number>)     (-|+)
    // default ("string")          (s)
    // default (<enumerator>)      (e)
    //
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    // Encode the kind of value we have in the first letter of
    // the string.
    //
    switch (tt)
    {
    case CPP_CLOSE_PAREN:
      {
        // Default value override.
        //
        break;
      }
    case CPP_STRING:
      {
        val = "s";
        val += TREE_STRING_POINTER (t);
        tt = pragma_lex (&t);
        break;
      }
    case CPP_NAME:
      {
        // This can be the null, true, or false keyword or a enumerator
        // name.
        //
        string n (IDENTIFIER_POINTER (t));

        if (n == "null" || n == "true" || n == "false")
        {
          val = n[0];
          tt = pragma_lex (&t);
          break;
        }
        // Fall throught.
      }
    case CPP_SCOPE:
      {
        // We have a potentially scopped enumerator name.
        //
        string n;
        tree decl = parse_scoped_name (t, tt, n, false, p);

        if (decl == 0)
          return;

        node = decl;
        val = "e" + n;
        break;
      }
    case CPP_MINUS:
    case CPP_PLUS:
      {
        val = (tt == CPP_MINUS ? "-" : "+");
        tt = pragma_lex (&t);

        if (tt != CPP_NUMBER)
        {
          error () << "expected numeric constant after '" << val
                   << "' in db pragma '" << p << "'" << endl;
          return;
        }

        // Fall through.
      }
    case CPP_NUMBER:
      {
        int tc (TREE_CODE (t));

        if (tc != INTEGER_CST && tc != REAL_CST)
        {
          error () << "unexpected numeric constant in db pragma '" << p
                   << "'" << endl;
          return;
        }

        if (val.empty ())
          val = "+";

        node = t;
        tt = pragma_lex (&t);
        break;
      }
    default:
      {
        error () << "unexpected expression in db pragma '" << p << "'" << endl;
        return;
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
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
      error () << "'(' expected after db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_NAME)
    {
      error () << "member name expected in db pragma '" << p << "'" << endl;
      return;
    }

    val = IDENTIFIER_POINTER (t);

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma '" << p << "'" << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "unordered")
  {
    // unordered
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_decl_type (decl, decl_name, p, loc))
      return;

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
    error () << "unknown db pragma '" << p << "'" << endl;
    return;
  }

  // Record this pragma.
  //
  pragma prag (mode, p, val, node, loc);

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
    error () << "unexpected text after '" << p << "' in db pragma" << endl;
}

static void
handle_pragma_qualifier (cpp_reader* reader, string const& p)
{
  tree t;
  cpp_ttype tt;

  tree decl (0);
  string decl_name;
  location_t loc (input_location);

  // Pragma qualifiers.
  //
  if (p == "object" ||
      p == "view" ||
      p == "value")
  {
    // object [(<identifier>)]
    // view [(<identifier>)]
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
          error () << "')' expected at the end of db pragma '" << p << "'"
                   << endl;
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error () << "type name expected in db pragma '" << p << "'" << endl;
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
          error () << "')' expected at the end of db pragma '" << p << "'"
                   << endl;
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error () << "data member name expected in db pragma '" << p << "'"
                 << endl;
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
           p == "options" ||
           p == "value_options" ||
           p == "index_options" ||
           p == "key_options" ||
           p == "id_options" ||
           p == "type" ||
           p == "value_type" ||
           p == "index_type" ||
           p == "key_type" ||
           p == "table" ||
           p == "null" ||
           p == "not_null" ||
           p == "value_null" ||
           p == "value_not_null" ||
           p == "default" ||
           p == "inverse" ||
           p == "unordered" ||
           p == "transient")
  {
    handle_pragma (reader, p, 0, "");
    return;
  }
  else
  {
    error () << "unknown db pragma '" << p << "'" << endl;
    return;
  }

  // Record this pragma.
  //
  pragma prag (pragma::override, p, "", 0, loc);

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
    error () << "unexpected text after " << p << " in db pragma" << endl;
}

extern "C" void
handle_pragma_db_object (cpp_reader* r)
{
  handle_pragma_qualifier (r, "object");
}

extern "C" void
handle_pragma_db_view (cpp_reader* r)
{
  handle_pragma_qualifier (r, "view");
}

extern "C" void
handle_pragma_db_value (cpp_reader* r)
{
  handle_pragma_qualifier (r, "value");
}

extern "C" void
handle_pragma_db_member (cpp_reader* r)
{
  handle_pragma_qualifier (r, "member");
}

extern "C" void
handle_pragma_db_id (cpp_reader* r)
{
  handle_pragma_qualifier (r, "id");
}

extern "C" void
handle_pragma_db_auto (cpp_reader* r)
{
  handle_pragma_qualifier (r, "auto");
}

extern "C" void
handle_pragma_db_column (cpp_reader* r)
{
  handle_pragma_qualifier (r, "column");
}

extern "C" void
handle_pragma_db_vcolumn (cpp_reader* r)
{
  handle_pragma_qualifier (r, "value_column");
}

extern "C" void
handle_pragma_db_icolumn (cpp_reader* r)
{
  handle_pragma_qualifier (r, "index_column");
}

extern "C" void
handle_pragma_db_kcolumn (cpp_reader* r)
{
  handle_pragma_qualifier (r, "key_column");
}

extern "C" void
handle_pragma_db_idcolumn (cpp_reader* r)
{
  handle_pragma_qualifier (r, "id_column");
}

extern "C" void
handle_pragma_db_options (cpp_reader* r)
{
  handle_pragma_qualifier (r, "options");
}

extern "C" void
handle_pragma_db_voptions (cpp_reader* r)
{
  handle_pragma_qualifier (r, "value_options");
}

extern "C" void
handle_pragma_db_ioptions (cpp_reader* r)
{
  handle_pragma_qualifier (r, "index_options");
}

extern "C" void
handle_pragma_db_koptions (cpp_reader* r)
{
  handle_pragma_qualifier (r, "key_options");
}

extern "C" void
handle_pragma_db_idoptions (cpp_reader* r)
{
  handle_pragma_qualifier (r, "id_options");
}

extern "C" void
handle_pragma_db_type (cpp_reader* r)
{
  handle_pragma_qualifier (r, "type");
}

extern "C" void
handle_pragma_db_id_type (cpp_reader* r)
{
  handle_pragma_qualifier (r, "id_type");
}

extern "C" void
handle_pragma_db_vtype (cpp_reader* r)
{
  handle_pragma_qualifier (r, "value_type");
}

extern "C" void
handle_pragma_db_itype (cpp_reader* r)
{
  handle_pragma_qualifier (r, "index_type");
}

extern "C" void
handle_pragma_db_ktype (cpp_reader* r)
{
  handle_pragma_qualifier (r, "key_type");
}

extern "C" void
handle_pragma_db_table (cpp_reader* r)
{
  handle_pragma_qualifier (r, "table");
}

extern "C" void
handle_pragma_db_null (cpp_reader* r)
{
  handle_pragma_qualifier (r, "null");
}

extern "C" void
handle_pragma_db_not_null (cpp_reader* r)
{
  handle_pragma_qualifier (r, "not_null");
}

extern "C" void
handle_pragma_db_value_null (cpp_reader* r)
{
  handle_pragma_qualifier (r, "value_null");
}

extern "C" void
handle_pragma_db_value_not_null (cpp_reader* r)
{
  handle_pragma_qualifier (r, "value_not_null");
}

extern "C" void
handle_pragma_db_default (cpp_reader* r)
{
  handle_pragma_qualifier (r, "default");
}

extern "C" void
handle_pragma_db_inverse (cpp_reader* r)
{
  handle_pragma_qualifier (r, "inverse");
}

extern "C" void
handle_pragma_db_unordered (cpp_reader* r)
{
  handle_pragma_qualifier (r, "unordered");
}

extern "C" void
handle_pragma_db_transient (cpp_reader* r)
{
  handle_pragma_qualifier (r, "transient");
}

extern "C" void
handle_pragma_db (cpp_reader* r)
{
  tree t;
  cpp_ttype tt (pragma_lex (&t));

  if (tt != CPP_NAME)
  {
    if (tt == CPP_EOF)
      error () << "expected specifier after db pragma" << endl;
    else
      error () << "unexpected token after db pragma" << endl;
    return;
  }

  handle_pragma_qualifier (r, IDENTIFIER_POINTER (t));
}

extern "C" void
register_odb_pragmas (void*, void*)
{
  // GCC has a limited number of pragma slots and we have exhausted them.
  // A workaround is to make 'db' a pragma rather than a namespace. This
  // way we only have one pragma but the drawback of this approach is the
  // fact that the specifier or qualifier name will now be macro-expanded
  // (though this happens anyway if we have multiple specifiers in a single
  // pragma). Once the GCC folks fix this, we can go back to the namespace
  // approach.
  //
  c_register_pragma_with_expansion (0, "db", handle_pragma_db);

  /*
  c_register_pragma_with_expansion ("db", "object", handle_pragma_db_object);
  c_register_pragma_with_expansion ("db", "view", handle_pragma_db_view);
  c_register_pragma_with_expansion ("db", "value", handle_pragma_db_value);
  c_register_pragma_with_expansion ("db", "member", handle_pragma_db_member);
  c_register_pragma_with_expansion ("db", "id", handle_pragma_db_id);
  c_register_pragma_with_expansion ("db", "auto", handle_pragma_db_auto);
  c_register_pragma_with_expansion ("db", "column", handle_pragma_db_column);
  c_register_pragma_with_expansion ("db", "value_column", handle_pragma_db_vcolumn);
  c_register_pragma_with_expansion ("db", "index_column", handle_pragma_db_icolumn);
  c_register_pragma_with_expansion ("db", "key_column", handle_pragma_db_kcolumn);
  c_register_pragma_with_expansion ("db", "id_column", handle_pragma_db_idcolumn);
  c_register_pragma_with_expansion ("db", "options", handle_pragma_db_options);
  c_register_pragma_with_expansion ("db", "value_options", handle_pragma_db_voptions);
  c_register_pragma_with_expansion ("db", "index_options", handle_pragma_db_ioptions);
  c_register_pragma_with_expansion ("db", "key_options", handle_pragma_db_koptions);
  c_register_pragma_with_expansion ("db", "id_options", handle_pragma_db_idoptions);
  c_register_pragma_with_expansion ("db", "type", handle_pragma_db_type);
  c_register_pragma_with_expansion ("db", "id_type", handle_pragma_db_id_type);
  c_register_pragma_with_expansion ("db", "value_type", handle_pragma_db_vtype);
  c_register_pragma_with_expansion ("db", "index_type", handle_pragma_db_itype);
  c_register_pragma_with_expansion ("db", "key_type", handle_pragma_db_ktype);
  c_register_pragma_with_expansion ("db", "table", handle_pragma_db_table);
  c_register_pragma_with_expansion ("db", "null", handle_pragma_db_null);
  c_register_pragma_with_expansion ("db", "not_null", handle_pragma_db_not_null);
  c_register_pragma_with_expansion ("db", "value_null", handle_pragma_db_value_null);
  c_register_pragma_with_expansion ("db", "value_not_null", handle_pragma_db_value_not_null);
  c_register_pragma_with_expansion ("db", "default", handle_pragma_db_default);
  c_register_pragma_with_expansion ("db", "inverse", handle_pragma_db_inverse);
  c_register_pragma_with_expansion ("db", "unordered", handle_pragma_db_unordered);
  c_register_pragma_with_expansion ("db", "transient", handle_pragma_db_transient);
  */
}
