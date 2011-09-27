// file      : odb/pragma.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <cctype> // std::isalnum
#include <vector>
#include <sstream>

#include <odb/diagnostics.hxx>
#include <odb/lookup.hxx>
#include <odb/pragma.hxx>
#include <odb/cxx-token.hxx>
#include <odb/cxx-lexer.hxx>
#include <odb/context.hxx>

using namespace std;
using namespace cutl;

using container::any;

template <typename X>
void
accumulate (compiler::context& ctx, string const& k, any const& v, location_t)
{
  // Empty values are used to indicate that this pragma must be ignored.
  //
  if (v.empty ())
    return;

  typedef vector<X> container;

  container& c (ctx.count (k)
                ? ctx.get<container> (k)
                : ctx.set (k, container ()));

  c.push_back (v.value<X> ());
}

// Lists of pragmas.
//
loc_pragmas loc_pragmas_;
decl_pragmas decl_pragmas_;

static bool
parse_expression (tree& t,
                  cpp_ttype& tt,
                  cxx_tokens& ts,
                  string const& prag)
{
  // Keep reading tokens until we see a matching ')' while keeping track
  // of their balance. Also switch to the pragma lexer so that we detect
  // C++ keywords (this is a C++ expression).
  //
  size_t balance (0);
  cxx_pragma_lexer lex;
  lex.start (t, tt);

  for (; tt != CPP_EOF; tt = lex.next (t))
  {
    bool done (false);
    cxx_token ct;

    switch (tt)
    {
    case CPP_OPEN_PAREN:
      {
        balance++;
        break;
      }
    case CPP_CLOSE_PAREN:
      {
        if (balance == 0)
          done = true;
        else
          balance--;
        break;
      }
    case CPP_STRING:
      {
        ct.literal = TREE_STRING_POINTER (t);
        break;
      }
    case CPP_NAME:
  //case CPP_KEYWORD: see default:
      {
        ct.literal = IDENTIFIER_POINTER (t);
        break;
      }
    case CPP_NUMBER:
      {
        switch (TREE_CODE (t))
        {
        case INTEGER_CST:
          {
            tree type (TREE_TYPE (t));

            HOST_WIDE_INT hwl (TREE_INT_CST_LOW (t));
            HOST_WIDE_INT hwh (TREE_INT_CST_HIGH (t));

            unsigned long long l (hwl);
            unsigned long long h (hwh);
            unsigned short width (HOST_BITS_PER_WIDE_INT);

            unsigned long long v ((h << width) + l);

            ostringstream os;
            os << v;

            if (type == long_long_integer_type_node)
              os << "LL";
            else if (type == long_long_unsigned_type_node)
              os << "ULL";
            else if (type == long_integer_type_node)
              os << "L";
            else if (type == long_unsigned_type_node)
              os << "UL";
            else if (
              TYPE_UNSIGNED (type) &&
              TYPE_PRECISION (type) >= TYPE_PRECISION (integer_type_node))
              os << "U";

            ct.literal = os.str ();
            break;
          }
        case REAL_CST:
          {
            tree type (TREE_TYPE (t));
            REAL_VALUE_TYPE val (TREE_REAL_CST (t));

            // This is the best we can do. val cannot be INF or NaN.
            //
            char tmp[256];
            real_to_decimal (tmp, &val, sizeof (tmp), 0, true);
            istringstream is (tmp);
            ostringstream os;

            if (type == float_type_node)
            {
              float f;
              is >> f;
              os << f << 'F';
            }
            else
            {
              double d;
              is >> d;
              os << d;
            }

            ct.literal = os.str ();
            break;
          }
        default:
          {
            error ()
              << "unexpected numeric constant in db pragma " << prag << endl;
            return false;
          }
        }

        break;
      }
    default:
      {
        // CPP_KEYWORD is not in the cpp_ttype enumeration.
        //
        if (tt == CPP_KEYWORD)
          ct.literal = IDENTIFIER_POINTER (t);

        break;
      }
    }

    if (done)
      break;

    ct.type = tt;
    ts.push_back (ct);
  }

  return true;
}


static string
parse_scoped_name (tree& token, cpp_ttype& type, string const& prag)
{
  try
  {
    cxx_pragma_lexer lex;
    string st (lex.start (token, type));
    return lookup::parse_scoped_name (st, type, lex);
  }
  catch (lookup::invalid_name const&)
  {
    error () << "invalid name in db pragma " << prag << endl;
    return "";
  }
}

static tree
resolve_scoped_name (tree& token,
                     cpp_ttype& type,
                     string& name,
                     bool is_type,
                     string const& prag)
{
  try
  {
    cxx_pragma_lexer lex;
    cpp_ttype ptt; // Not used.
    string st (lex.start (token, type));

    tree decl (
      lookup::resolve_scoped_name (
        st, type, ptt, lex, current_scope (), name, is_type));

    // Get the actual type if this is a TYPE_DECL.
    //
    if (is_type)
    {
      if (TREE_CODE (decl) == TYPE_DECL)
        decl = TREE_TYPE (decl);

      if (TYPE_P (decl)) // Can be a template.
        decl = TYPE_MAIN_VARIANT (decl);
    }

    return decl;
  }
  catch (lookup::invalid_name const&)
  {
    error () << "invalid name in db pragma " << prag << endl;
    return 0;
  }
  catch (lookup::unable_to_resolve const& e)
  {
    if (e.last ())
      error () << "unable to resolve " << (is_type ? "type " : "") << "name "
               << "'" << e.name () << "' in db pragma " << prag << endl;
    else
      error () << "unable to resolve name '" << e.name () << "' in db pragma "
               << prag << endl;

    return 0;
  }
}

static bool
check_qual_decl_type (tree d,
                      string const& name,
                      string const& p,
                      location_t l)
{
  int tc (TREE_CODE (d));

  if (p == "member")
  {
    if (tc != FIELD_DECL)
    {
      error (l) << "name '" << name << "' in db pragma " << p << " does "
                << "not refer to a data member" << endl;
      return false;
    }
  }
  else if (p == "object" ||
           p == "view")
  {
    if (tc != RECORD_TYPE)
    {
      error (l) << "name '" << name << "' in db pragma " << p << " does "
                << "not refer to a class" << endl;
      return false;
    }
  }
  else if (p == "value")
  {
    if (!TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma " << p << " does "
                << "not refer to a type" << endl;
      return false;
    }
  }
  else
  {
    error () << "unknown db pragma " << p << endl;
    return false;
  }

  return true;
}

static bool
check_spec_decl_type (tree d,
                      string const& name,
                      string const& p,
                      location_t l)
{
  int tc (TREE_CODE (d));

  if (p == "id" ||
      p == "auto" ||
      p == "column" ||
      p == "inverse" ||
      p == "transient")
  {
    if (tc != FIELD_DECL)
    {
      error (l) << "name '" << name << "' in db pragma " << p << " does "
                << "not refer to a data member" << endl;
      return false;
    }
  }
  else if (p == "pointer" ||
           p == "abstract" ||
           p == "callback" ||
           p == "query" ||
           p == "object")
  {
    if (tc != RECORD_TYPE)
    {
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else if (p == "id_type")
  {
    // Id type can only be used for types.
    //
    if (!TYPE_P (d))
    {
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
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
      error (l) << "name '" << name << "' in db pragma " << p << " does "
                << "not refer to a type or data member" << endl;
      return false;
    }
  }
  else
  {
    error () << "unknown db pragma " << p << endl;
    return false;
  }

  return true;
}

static void
add_pragma (pragma const& prag, tree decl)
{
  if (decl)
    decl_pragmas_[decl].insert (prag);
  else
  {
    tree scope (current_scope ());

    if (!CLASS_TYPE_P (scope))
      scope = global_namespace;

    loc_pragmas_[scope].push_back (prag);
  }
}

static void
handle_pragma (cpp_reader* reader,
               string const& p,
               tree decl,
               string const& decl_name)
{
  tree t;
  cpp_ttype tt;

  string name (p);                           // Pragma name.
  any val;                                   // Pragma value.
  pragma::add_func adder (0);                // Custom context adder.
  location_t loc (input_location);           // Pragma location.

  if (p == "table")
  {
    // table ("<name>")
    // table ("<name>" [= "<alias>"] [: "<cond>"]  (view only)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "table name expected in db pragma " << p << endl;
      return;
    }

    // The table specifier is used for both objects and views. In case
    // of an object, the context values is just a string. In case of a
    // view, the context value is a view_object entry. The problem is
    // that at this stage we don't know whether we are dealing with an
    // object or a view. To resolve this in a somewhat hackish way, we
    // are going to create both a string and a view_object entry.
    //
    view_object vo;
    vo.kind = view_object::table;
    vo.orig_name = TREE_STRING_POINTER (t);

    tt = pragma_lex (&t);

    if (tt == CPP_EQ)
    {
      // We have an alias.
      //
      if (pragma_lex (&t) != CPP_STRING)
      {
        error ()
          << "table alias expected after '=' in db pragma " << p << endl;
        return;
      }

      vo.alias = TREE_STRING_POINTER (t);
      tt = pragma_lex (&t);
    }

    if (tt == CPP_COLON)
    {
      // We have a condition.

      tt = pragma_lex (&t);

      if (!parse_expression (t, tt, vo.cond, p))
        return; // Diagnostics has already been issued.

      if (vo.cond.empty ())
      {
        error ()
          << "join condition expected after ':' in db pragma " << p << endl;
        return;
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    // Add the "table" pragma.
    //
    if (vo.alias.empty () && vo.cond.empty ())
      add_pragma (
        pragma (p, name, vo.orig_name, loc, &check_spec_decl_type, 0), decl);

    vo.scope = current_scope ();
    vo.loc = loc;
    val = vo;
    name = "objects";
    adder = &accumulate<view_object>;

    tt = pragma_lex (&t);
  }
  else if (p == "pointer")
  {
    // pointer (qname)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    class_pointer cp;
    size_t pb (0);
    bool punc (false);

    for (tt = pragma_lex (&t);
         tt != CPP_EOF && (tt != CPP_CLOSE_PAREN || pb != 0);
         tt = pragma_lex (&t))
    {
      if (punc && tt > CPP_LAST_PUNCTUATOR)
        cp.name += ' ';

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
          cp.name += "< ";
          break;
        }
      case CPP_GREATER:
        {
          cp.name += " >";
          break;
        }
      case CPP_COMMA:
        {
          cp.name += ", ";
          break;
        }
      case CPP_NAME:
        {
          cp.name += IDENTIFIER_POINTER (t);
          punc = true;
          break;
        }
      default:
        {
          if (tt <= CPP_LAST_PUNCTUATOR)
            cp.name += cxx_lexer::token_spelling[tt];
          else
          {
            error () << "unexpected token '" << cxx_lexer::token_spelling[tt]
                     << "' in db pragma " << p << endl;
            return;
          }
          break;
        }
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    if (cp.name.empty ())
    {
      error () << "expected pointer name in db pragma " << p << endl;
      return;
    }

    cp.scope = current_scope ();
    cp.loc = loc;
    val = cp;

    tt = pragma_lex (&t);
  }
  else if (p == "abstract")
  {
    // abstract
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "callback")
  {
    // callback (name)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_NAME)
    {
      error () << "member function name expected in db pragma " << p << endl;
      return;
    }

    val = string (IDENTIFIER_POINTER (t));

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "query")
  {
    // query ()
    // query ("statement")
    // query (expr)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    view_query vq;

    bool s (false);
    string str;

    if (tt == CPP_STRING)
    {
      s = true;
      str = TREE_STRING_POINTER (t);
      tt = pragma_lex (&t);
    }

    if (tt == CPP_CLOSE_PAREN)
    {
      if (s)
        vq.literal = str;
      else
      {
        // Empty query() pragma indicates that the statement will be
        // provided at runtime. Encode this case as empty literal
        // and expression.
        //
      }
    }
    else
    {
      // Expression.
      //
      if (s)
      {
        vq.expr.push_back (cxx_token ());
        vq.expr.back ().type = CPP_STRING;
        vq.expr.back ().literal = str;
      }

      if (!parse_expression (t, tt, vq.expr, p))
        return; // Diagnostics has already been issued.
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    vq.scope = current_scope ();
    vq.loc = loc;
    val = vq;
    tt = pragma_lex (&t);
  }
  else if (p == "object")
  {
    // object (fq-name [ = name] [: expr])
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_NAME && tt != CPP_SCOPE)
    {
      error () << "type name expected in db pragma " << p << endl;
      return;
    }

    view_object vo;
    vo.kind = view_object::object;
    vo.node = resolve_scoped_name (t, tt, vo.orig_name, true, p);

    if (vo.node == 0)
      return; // Diagnostics has already been issued.

    if (tt == CPP_EQ)
    {
      // We have an alias.
      //
      if (pragma_lex (&t) != CPP_NAME)
      {
        error () << "alias name expected after '=' in db pragma " << p << endl;
        return;
      }

      vo.alias = IDENTIFIER_POINTER (t);
      tt = pragma_lex (&t);
    }

    if (tt == CPP_COLON)
    {
      // We have a condition.

      tt = pragma_lex (&t);

      if (!parse_expression (t, tt, vo.cond, p))
        return; // Diagnostics has already been issued.

      if (vo.cond.empty ())
      {
        error ()
          << "join condition expected after ':' in db pragma " << p << endl;
        return;
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    vo.scope = current_scope ();
    vo.loc = loc;
    val = vo;
    name = "objects"; // Change the context entry name.
    adder = &accumulate<view_object>;

    tt = pragma_lex (&t);
  }
  else if (p == "id")
  {
    // id
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "auto")
  {
    // auto
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "column")
  {
    // column ("<name>")
    // column ("<name>.<name>")
    // column ("<name>"."<name>")
    // column (fq-name)             (view only)
    // column (expr)                (view only)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    bool s (false);
    string str;

    // String can be just the column name, a table name followed by the
    // column name, or part of an expression, depending on what comes
    // after the string.
    //
    if (tt == CPP_STRING)
    {
      s = true;
      str = TREE_STRING_POINTER (t);
      tt = pragma_lex (&t);
    }

    if (tt == CPP_CLOSE_PAREN)
    {
      if (s)
      {
        // "<name>" or "<name>.<name>"
        //
        table_column tc;
        tc.expr = false;

        // Scan the string and see if we have any non-identifier
        // characters. If so, assume it is an expression. While
        // at it also see if there is '.'.
        //
        string::size_type p (string::npos);

        for (size_t i (0); i < str.size (); ++i)
        {
          char c (str[i]);

          if (!(isalnum (c) || c == '_'))
          {
            tc.expr = true;
            break;
          }

          if (c == '.')
          {
            if (p != string::npos)
            {
              // Second '.' -- something fishy is going on.
              tc.expr = true;
              break;
            }

            p  = i;
          }
        }

        if (!tc.expr && p != string::npos)
        {
          tc.table.assign (str, 0, p);
          tc.column.assign (str, p + 1, string::npos);
        }
        else
          tc.column = str;

        val = tc;
      }
      else
      {
        error () << "column name expected in db pragma " << p << endl;
        return;
      }
    }
    else if (tt == CPP_DOT)
    {
      if (s)
      {
        // "<name>"."<name>"
        //
        table_column tc;
        tc.expr = false;

        if (pragma_lex (&t) != CPP_STRING)
        {
          error () << "column name expected after '.' in db pragma " << p
                   << endl;
          return;
        }

        tc.table = str;
        tc.column = TREE_STRING_POINTER (t);
        val = tc;
        tt = pragma_lex (&t);
      }
      else
      {
        error () << "column name expected in db pragma " << p << endl;
        return;
      }
    }
    else
    {
      // We have an expression.
      //
      column_expr e;

      if (s)
      {
        e.push_back (column_expr_part ());
        e.back ().kind = column_expr_part::literal;
        e.back ().value = str;

        if (tt != CPP_PLUS)
        {
          error () << "'+' or ')' expected in db pragma " << p << endl;
          return;
        }

        tt = pragma_lex (&t);
      }

      for (;;)
      {
        if (tt == CPP_STRING)
        {
          e.push_back (column_expr_part ());
          e.back ().kind = column_expr_part::literal;
          e.back ().value = TREE_STRING_POINTER (t);

          tt = pragma_lex (&t);
        }
        else if (tt == CPP_NAME || tt == CPP_SCOPE)
        {
          string name (parse_scoped_name (t, tt, p));

          if (name.empty ())
            return; // Diagnostics has already been issued.

          // Resolve nested members if any.
          //
          for (; tt == CPP_DOT; tt = pragma_lex (&t))
          {
            if (pragma_lex (&t) != CPP_NAME)
            {
              error () << "name expected after '.' in db pragma " << p << endl;
              return;
            }

            name += '.';
            name += IDENTIFIER_POINTER (t);
          }

          e.push_back (column_expr_part ());
          e.back ().kind = column_expr_part::reference;
          e.back ().value = name;
          e.back ().scope = current_scope ();
          e.back ().loc = loc;
        }
        else
        {
          error () << "string literal or name expected in db pragma " << p
                   << endl;
          return;
        }

        if (tt == CPP_PLUS)
          tt = pragma_lex (&t);
        else if (tt == CPP_CLOSE_PAREN)
          break;
        else
        {
          error () << "'+' or ')' expected in db pragma " << p << endl;
          return;
        }
      }

      e.loc = loc;
      val = e;
      name = "column-expr";
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);
  }
  else if (p == "value_column" ||
           p == "index_column" ||
           p == "key_column" ||
           p == "id_column")
  {
    // value_column ("<name>")
    // index_column ("<name>")
    // key_column ("<name>")
    // id_column ("<name>")
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "column name expected in db pragma " << p << endl;
      return;
    }

    val = string (TREE_STRING_POINTER (t));

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
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
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt == CPP_STRING)
    {
      string o (TREE_STRING_POINTER (t));

      // Ignore empty options strings. Internally, we use them to
      // indicate options reset (see below).
      //
      if (!o.empty ())
        val = string (TREE_STRING_POINTER (t));

      tt = pragma_lex (&t);
    }
    else if (tt == CPP_CLOSE_PAREN)
    {
      // Empty options specifier signals options reset. Encode it as an
      // empty string.
      //
      val = string ();
    }
    else
    {
      error () << "options string expected in db pragma " << p << endl;
      return;
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    adder = &accumulate<string>;
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
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_STRING)
    {
      error () << "type name expected in db pragma " << p << endl;
      return;
    }

    val = string (TREE_STRING_POINTER (t));

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
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
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
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
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    default_value dv;

    switch (tt)
    {
    case CPP_CLOSE_PAREN:
      {
        // Default value reset.
        //
        dv.kind = default_value::reset;
        break;
      }
    case CPP_STRING:
      {
        dv.kind = default_value::string;
        dv.value = TREE_STRING_POINTER (t);
        tt = pragma_lex (&t);
        break;
      }
    case CPP_NAME:
      {
        // This can be the null, true, or false keyword or an enumerator
        // name.
        //
        string n (IDENTIFIER_POINTER (t));

        if (n == "null")
        {
          dv.kind = default_value::null;
          tt = pragma_lex (&t);
          break;
        }
        else if (n == "true" || n == "false")
        {
          dv.kind = default_value::boolean;
          dv.value = n;
          tt = pragma_lex (&t);
          break;
        }
        // Fall throught.
      }
    case CPP_SCOPE:
      {
        // We have a potentially scopped enumerator name.
        //
        dv.node = resolve_scoped_name (t, tt, dv.value, false, p);

        if (dv.node == 0)
          return; // Diagnostics has already been issued.

        dv.kind = default_value::enumerator;
        break;
      }
    case CPP_MINUS:
    case CPP_PLUS:
      {
        if (tt == CPP_MINUS)
          dv.value = "-";

        tt = pragma_lex (&t);

        if (tt != CPP_NUMBER)
        {
          error () << "expected numeric constant after '"
                   << (tt == CPP_MINUS ? "-" : "+") << "' in db pragma "
                   << p << endl;
          return;
        }

        // Fall through.
      }
    case CPP_NUMBER:
      {
        int tc (TREE_CODE (t));

        if (tc != INTEGER_CST && tc != REAL_CST)
        {
          error () << "unexpected numeric constant in db pragma " << p << endl;
          return;
        }

        dv.node = t;
        dv.kind = default_value::number;
        tt = pragma_lex (&t);
        break;
      }
    default:
      {
        error () << "unexpected expression in db pragma " << p << endl;
        return;
      }
    }

    if (tt != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
      return;
    }

    val = dv;
    tt = pragma_lex (&t);
  }
  else if (p == "inverse")
  {
    // inverse (name)
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    if (pragma_lex (&t) != CPP_OPEN_PAREN)
    {
      error () << "'(' expected after db pragma " << p << endl;
      return;
    }

    tt = pragma_lex (&t);

    if (tt != CPP_NAME)
    {
      error () << "member name expected in db pragma " << p << endl;
      return;
    }

    val = string (IDENTIFIER_POINTER (t));

    if (pragma_lex (&t) != CPP_CLOSE_PAREN)
    {
      error () << "')' expected at the end of db pragma " << p << endl;
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
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else if (p == "transient")
  {
    // transient
    //

    // Make sure we've got the correct declaration type.
    //
    if (decl != 0 && !check_spec_decl_type (decl, decl_name, p, loc))
      return;

    tt = pragma_lex (&t);
  }
  else
  {
    error () << "unknown db pragma " << p << endl;
    return;
  }

  // If the value is not specified and we don't use a custom adder,
  // then make it bool (flag).
  //
  if (adder == 0 && val.empty ())
    val = true;

  // Record this pragma.
  //
  add_pragma (pragma (p, name, val, loc, &check_spec_decl_type, adder), decl);

  // See if there are any more pragmas.
  //
  if (tt == CPP_NAME)
  {
    handle_pragma (reader, IDENTIFIER_POINTER (t), decl, decl_name);
  }
  else if (tt != CPP_EOF)
    error () << "unexpected text after " << p << " in db pragma" << endl;
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
        decl = resolve_scoped_name (t, tt, decl_name, true, p);

        if (decl == 0)
          return; // Diagnostics has already been issued.

        // Make sure we've got the correct declaration type.
        //
        if (!check_qual_decl_type (decl, decl_name, p, loc))
          return;

        if (tt != CPP_CLOSE_PAREN)
        {
          error () << "')' expected at the end of db pragma " << p << endl;
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error () << "type name expected in db pragma " << p << endl;
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
        decl = resolve_scoped_name (t, tt, decl_name, false, p);

        if (decl == 0)
          return; // Diagnostics has already been issued.

        // Make sure we've got the correct declaration type.
        //
        if (!check_qual_decl_type (decl, decl_name, p, loc))
          return;

        if (tt != CPP_CLOSE_PAREN)
        {
          error () << "')' expected at the end of db pragma " << p << endl;
          return;
        }

        tt = pragma_lex (&t);
      }
      else
      {
        error () << "data member name expected in db pragma " << p << endl;
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
    error () << "unknown db pragma " << p << endl;
    return;
  }

  // Record this pragma.
  //
  pragma prag (p, p, any (true), loc, &check_qual_decl_type, 0);

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
