// file      : odb/relational/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <odb/lookup.hxx>
#include <odb/cxx-lexer.hxx>

#include <odb/relational/source.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

namespace relational
{
  namespace source
  {
    static inline void
    add_space (string& s)
    {
      string::size_type n (s.size ());
      if (n != 0 && s[n - 1] != ' ')
        s += ' ';
    }

    static string
    translate_name_trailer (string& t,
                            cpp_ttype& tt,
                            cpp_ttype& ptt,
                            cxx_tokens_lexer& lex)
    {
      string r;

      for (; tt != CPP_EOF; ptt = tt, tt = lex.next (t))
      {
        bool done (false);

        switch (tt)
        {
        case CPP_SCOPE:
        case CPP_DOT:
          {
            r += cxx_lexer::token_spelling[tt];
            break;
          }
        default:
          {
            // Handle CPP_KEYWORD here to avoid a warning (it is not
            // part of the cpp_ttype enumeration).
            //
            if (tt == CPP_NAME || tt == CPP_KEYWORD)
            {
              // For names like 'foo::template bar'.
              //
              if (ptt == CPP_NAME || ptt == CPP_KEYWORD)
                r += ' ';

              r += t;
            }
            else
              done = true;

            break;
          }
        }

        if (done)
          break;
      }

      return r;
    }

    static class_::expression
    translate_name (string& t,
                    cpp_ttype& tt,
                    cpp_ttype& ptt,
                    cxx_tokens_lexer& lex,
                    tree scope,
                    location_t loc,
                    string const& prag,
                    bool check_ptr,
                    view_alias_map const& amap,
                    view_object_map const& omap)
    {
      using semantics::data_member;
      typedef class_::expression expression;

      bool multi_obj ((amap.size () + omap.size ()) > 1);

      string r ("query_type");
      string name;

      bool fail (false);
      context& ctx (context::current ());

      // This code is quite similar to view_data_members in the type
      // processor.
      //
      try
      {
        tree decl (0);
        view_object* vo (0);

        // Check if this is an alias.
        //
        if (tt == CPP_NAME)
        {
          view_alias_map::const_iterator i (amap.find (t));

          if (i != amap.end ())
          {
            if (multi_obj)
            {
              r += "::";
              r += i->first;
            }

            vo = i->second;
            fail = true; // This must be a data member.

            // Skip '::'.
            //
            ptt = tt;
            tt = lex.next (t);

            if (tt != CPP_SCOPE)
            {
              error (loc)
                << "member name expected after an alias in db pragma "
                << prag << endl;
              throw operation_failed ();
            }

            ptt = tt;
            tt = lex.next (t);

            decl = lookup::resolve_scoped_name (
              t, tt, ptt, lex, vo->obj->tree_node (), name, false);
          }
        }

        // If it is not an alias, do the normal lookup.
        //
        if (vo == 0)
        {
          // Also get the object type. We need to do it so that
          // we can get the correct (derived) object name (the
          // member can come from a base class).
          //
          tree type;
          decl = lookup::resolve_scoped_name (
            t, tt, ptt, lex, scope, name, false, &type);

          type = TYPE_MAIN_VARIANT (type);

          view_object_map::const_iterator i (omap.find (type));

          if (i == omap.end ())
          {
            // Not an object associated with this view. Assume it
            // is some other valid name.
            //
            return expression (
              name + translate_name_trailer (t, tt, ptt, lex));
          }

          vo = i->second;

          if (multi_obj)
          {
            r += "::";
            r += vo->obj->name ();
          }
        }

        // Check that we have a data member.
        //
        if (TREE_CODE (decl) != FIELD_DECL)
        {
          if (fail)
          {
            error (loc)
              << "name '" << name << "' in db pragma " << prag << " "
              << "does not refer to a data member" << endl;
            throw operation_failed ();
          }
          else
            return expression (
              name + translate_name_trailer (t, tt, ptt, lex));
        }

        expression e (vo);

        data_member* m (dynamic_cast<data_member*> (ctx.unit.find (decl)));

        r += "::";
        r += ctx.public_name (*m);

        // Assemble the member path if we may need to return a pointer
        // expression.
        //
        if (check_ptr)
          e.member_path.push_back (m);

        fail = true; // Now we definitely fail if anything goes wrong.

        // Finally, resolve nested members if any.
        //
        for (; tt == CPP_DOT; ptt = tt, tt = lex.next (t))
        {
          // Check if this member is actually of a composite value type.
          // This is to handle expressions like "object::member.is_null ()"
          // correctly. The remaining issue here is that in the future
          // is_null()/is_not_null() will be valid for composite values
          // as well.
          //
          if (!context::composite_wrapper (context::utype (*m)))
            break;

          ptt = tt;
          tt = lex.next (t);

          if (tt != CPP_NAME)
          {
            error (loc)
              << "name expected after '.' in db pragma " << prag << endl;
            throw operation_failed ();
          }

          tree type (TYPE_MAIN_VARIANT (TREE_TYPE (decl)));

          decl = lookup_qualified_name (
            type, get_identifier (t.c_str ()), false, false);

          if (decl == error_mark_node || TREE_CODE (decl) != FIELD_DECL)
          {
            error (loc)
              << "name '" << t << "' in db pragma " << prag << " does not "
              << "refer to a data member" << endl;
            throw operation_failed ();
          }

          m = dynamic_cast<data_member*> (ctx.unit.find (decl));

          r += '.';
          r += ctx.public_name (*m);

          if (check_ptr)
            e.member_path.push_back (m);
        }

        // If requested, check if this member is a pointer. We only do this
        // if there is nothing after this name.
        //
        if (check_ptr && tt == CPP_EOF)
        {
          using semantics::type;

          type* t;

          if (type* c = context::container (*m))
            t = &context::container_vt (*c);
          else
            t = &context::utype (*m);

          if (context::object_pointer (*t))
            return e;
        }

        // Read the remainder of the expression (e.g., '.is_null ()') if
        // the member is not composite and we bailed out from the above
        // loop.
        //
        if (tt == CPP_DOT)
          r += translate_name_trailer (t, tt, ptt, lex);

        return expression (r);
      }
      catch (lookup::invalid_name const&)
      {
        if (fail)
        {
          error (loc) << "invalid name in db pragma " << prag << endl;
          throw operation_failed ();
        }
        else
          return expression (
            name + translate_name_trailer (t, tt, ptt, lex));
      }
      catch (lookup::unable_to_resolve const& e)
      {
        if (fail)
        {
          error (loc) << "unable to resolve name '" << e.name ()
                      << "' in db pragma " << prag << endl;
          throw operation_failed ();
        }
        else
          return expression (
            name + translate_name_trailer (t, tt, ptt, lex));
      }
    }

    class_::expression class_::
    translate_expression (type& c,
                          cxx_tokens const& ts,
                          tree scope,
                          location_t loc,
                          string const& prag,
                          bool* placeholder)
    {
      // The overall idea is as folows: read in tokens and add them
      // to the string. If a token starts a name, try to resolve it
      // to an object member (taking into account aliases). If this
      // was successful, translate it to the query column reference.
      // Otherwise, output it as is.
      //
      // If the placeholder argument is not NULL, then we need to
      // detect the special '(?)' token sequence and replace it
      // with the query variable ('q').
      //
      expression e ("");
      string& r (e.value);

      view_alias_map const& amap (c.get<view_alias_map> ("alias-map"));
      view_object_map const& omap (c.get<view_object_map> ("object-map"));

      cxx_tokens_lexer lex;
      lex.start (ts);

      string t;
      for (cpp_ttype tt (lex.next (t)), ptt (CPP_EOF); tt != CPP_EOF;)
      {
        // Try to format the expression to resemble the style of the
        // generated code.
        //
        switch (tt)
        {
        case CPP_NOT:
          {
            add_space (r);
            r += '!';
            break;
          }
        case CPP_COMMA:
          {
            r += ", ";
            break;
          }
        case CPP_OPEN_PAREN:
          {
            if (ptt == CPP_NAME ||
                ptt == CPP_KEYWORD)
              add_space (r);

            r += '(';
            break;
          }
        case CPP_CLOSE_PAREN:
          {
            r += ')';
            break;
          }
        case CPP_OPEN_SQUARE:
          {
            r += '[';
            break;
          }
        case CPP_CLOSE_SQUARE:
          {
            r += ']';
            break;
          }
        case CPP_OPEN_BRACE:
          {
            add_space (r);
            r += "{ ";
            break;
          }
        case CPP_CLOSE_BRACE:
          {
            add_space (r);
            r += '}';
            break;
          }
        case CPP_SEMICOLON:
          {
            r += ';';
            break;
          }
        case CPP_ELLIPSIS:
          {
            add_space (r);
            r += "...";
            break;
          }
        case CPP_PLUS:
        case CPP_MINUS:
          {
            bool unary (ptt != CPP_NAME &&
                        ptt != CPP_SCOPE &&
                        ptt != CPP_NUMBER &&
                        ptt != CPP_STRING &&
                        ptt != CPP_CLOSE_PAREN &&
                        ptt != CPP_PLUS_PLUS &&
                        ptt != CPP_MINUS_MINUS);

            if (!unary)
              add_space (r);

            r += cxx_lexer::token_spelling[tt];

            if (!unary)
              r += ' ';
            break;
          }
        case CPP_PLUS_PLUS:
        case CPP_MINUS_MINUS:
          {
            if (ptt != CPP_NAME &&
                ptt != CPP_CLOSE_PAREN &&
                ptt != CPP_CLOSE_SQUARE)
              add_space (r);

            r += cxx_lexer::token_spelling[tt];
            break;
          }
        case CPP_DEREF:
        case CPP_DEREF_STAR:
        case CPP_DOT:
        case CPP_DOT_STAR:
          {
            r += cxx_lexer::token_spelling[tt];
            break;
          }
        case CPP_STRING:
          {
            if (ptt == CPP_NAME ||
                ptt == CPP_KEYWORD ||
                ptt == CPP_STRING ||
                ptt == CPP_NUMBER)
              add_space (r);

            r += strlit (t);
            break;
          }
        case CPP_NUMBER:
          {
            if (ptt == CPP_NAME ||
                ptt == CPP_KEYWORD ||
                ptt == CPP_STRING ||
                ptt == CPP_NUMBER)
              add_space (r);

            r += t;
            break;
          }
        case CPP_SCOPE:
        case CPP_NAME:
          {
            // Start of a name.
            //
            if (ptt == CPP_NAME ||
                ptt == CPP_KEYWORD ||
                ptt == CPP_STRING ||
                ptt == CPP_NUMBER)
              add_space (r);

            // Check if this is a pointer expression.
            //
            // If r is not empty, then it means this is not just the
            // name. If placeholder is not 0, then we are translating
            // a query expression, not a join condition.
            //
            expression e (
              translate_name (
                t, tt, ptt, lex,
                scope, loc, prag,
                r.empty () && placeholder == 0, amap, omap));

            if (e.kind == expression::literal)
              r += e.value;
            else
              return e;

            continue; // We have already extracted the next token.
          }
        case CPP_QUERY:
          {
            if (placeholder != 0 && !*placeholder)
            {
              if (ptt == CPP_OPEN_PAREN)
              {
                // Get the next token and see if it is ')'.
                //
                ptt = tt;
                tt = lex.next (t);

                if (tt == CPP_CLOSE_PAREN)
                {
                  r +=  "q.empty () ? query_base_type::true_expr : q";
                  *placeholder = true;
                }
                else
                {
                  // The same as in the default case.
                  //
                  add_space (r);
                  r += "? ";
                }
                continue; // We have already gotten the next token.
              }
            }
            // Fall through.
          }
        default:
          {
            // Handle CPP_KEYWORD here to avoid a warning (it is not
            // part of the cpp_ttype enumeration).
            //
            if (tt == CPP_KEYWORD)
            {
              if (ptt == CPP_NAME ||
                  ptt == CPP_KEYWORD ||
                  ptt == CPP_STRING ||
                  ptt == CPP_NUMBER)
                add_space (r);

              r += t;
            }
            else
            {
              // All the other operators.
              //
              add_space (r);
              r += cxx_lexer::token_spelling[tt];
              r += ' ';
            }
            break;
          }
        }

        //
        // Watch out for the continue statements above if you add any
        // logic here.
        //

        ptt = tt;
        tt = lex.next (t);
      }

      return e;
    }

    void
    generate ()
    {
      context ctx;
      ostream& os (ctx.os);

      traversal::unit unit;
      traversal::defines unit_defines;
      traversal::namespace_ ns;
      instance<class_> c;

      unit >> unit_defines >> ns;
      unit_defines >> c;

      traversal::defines ns_defines;

      ns >> ns_defines >> ns;
      ns_defines >> c;

      instance<include> i;
      i->generate ();

      os << "namespace odb"
         << "{";

      unit.dispatch (ctx.unit);

      os << "}";
    }
  }
}
