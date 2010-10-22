// file      : odb/include.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <set>
#include <map>
#include <locale>
#include <cassert>
#include <fstream>

#include <odb/include.hxx>

#include <iostream>

using namespace std;
using semantics::path;

namespace
{
  struct include
  {
    enum type { quote, bracket };

    type type_;
    path path_;
  };

  typedef std::map<line_map const*, include> includes;
  typedef std::map<path, includes> include_map;

  // Map of files to the lines which contain include directives
  // that we are interested in.
  //
  typedef std::map<size_t, include*> include_lines;
  typedef std::map<path, include_lines> file_map;

  // Set of include directives sorted in the preference order.
  //
  struct include_comparator
  {
    bool
    operator() (include const* x, include const* y) const
    {
      // Prefer <> over "".
      //
      if (x->type_ != y->type_)
        return x->type_ < y->type_;

      // Otherwise, prefer longer (more qualified) paths over the
      // shorter ones.
      //
      return x->path_.string ().size () < y->path_.string ().size ();
    }
  };

  typedef std::multiset<include const*, include_comparator> include_set;


  struct class_: traversal::class_, context
  {
    class_ (context& c, include_map& map)
        : context (c), map_ (map)
    {
    }

    virtual void
    traverse (type& c)
    {
      // Not interested in classes that we are generating.
      //
      if (c.file () == unit.file ())
        return;

      if (!(c.count ("object") || c.count ("value")))
        return;

      // This is a persistent object or value type declared in another
      // header file. Include its -odb header.
      //
      tree decl (TYPE_NAME (c.tree_node ()));
      location_t l (DECL_SOURCE_LOCATION (decl));

      if (l > BUILTINS_LOCATION)
      {
        line_map const* lm (linemap_lookup (line_table, l));

        if (lm != 0 && !MAIN_FILE_P (lm))
        {
          lm = INCLUDED_FROM (line_table, lm);

          path f (c.file ());
          f.complete ();
          f.normalize ();

          if (map_.find (f) == map_.end ())
            map_[f][lm] = include ();
        }
      }
    }

  private:
    include_map& map_;
  };

  class include_parser
  {
  public:
    include_parser ()
        : loc_ ("C")
    {
    }

    void
    parse_file (path const& f, include_lines& lines)
    {
      size_t lmax (lines.rbegin ()->first);
      ifstream is (f.string ().c_str ());

      if (!is.is_open ())
      {
        cerr << "error: unable to open '" << f << "' in read mode" << endl;
        throw generation_failed ();
      }

      typedef char_traits<char>::int_type int_type;

      string line;
      bool bslash (false);
      size_t lb (1), le (1);

      for (int_type c (is.get ()); is.good (); c = is.get ())
      {
        if (c == '\n')
        {
          le++;

          if (!bslash)
          {
            //cerr << "line: " << lb << "-" << (le - 1) << " " << line << endl;

            // See if we are interested in this range of physical lines.
            //
            include_lines::iterator li (lines.lower_bound (lb));
            include_lines::iterator ui (lines.upper_bound (le - 1));

            // We should have at most one entry per logical line.
            //
            for (; li != ui; ++li)
            {
              if (li->first >= lb && li->first <= (le - 1))
              {
                if (!parse_line (line, *li->second))
                {
                  cerr << f << ":" << lb << ":1: error: "
                       << "unable to parse #include directive" << endl;
                  throw generation_failed ();
                }
              }
            }

            if (le > lmax)
              break;

            lb = le;
            line.clear ();
          }

          bslash = false;
          continue;
        }

        if (bslash)
        {
          line += '\\';
          bslash = false;
        }

        if (c == '\\')
          bslash = true;
        else
        {
          line += char (c);
        }
      }

      if (is.bad () || (is.fail () && !is.eof ()))
      {
        cerr << "error: input error while reading '" << f << "'" << endl;
        throw generation_failed ();
      }
    }

  private:
    bool
    parse_line (string const& l, include& inc)
    {
      enum state
      {
        start_hash,
        start_keyword,
        parse_keyword,
        start_path,
        parse_path,
        parse_done
      };

      bool com (false);  // In C-style comment.
      string lex;
      char path_end;
      state s (start_hash);

      for (size_t i (0), n (l.size ()); i < n; ++i)
      {
        char c (l[i]);

        if (com)
        {
          if (c == '*' && (i + 1) < n && l[i + 1] == '/')
          {
            ++i;
            com = false;
            c = ' '; // Replace a comment with a single space.
          }
          else
            continue;
        }

        // We only ignore spaces in start states.
        //
        if (is_space (c))
        {
          switch (s)
          {
          case start_hash:
          case start_keyword:
          case start_path:
            {
              continue;
            }
          default:
            {
              break;
            }
          }
        }

        // C comment can be anywhere except in the path.
        //
        if (s != parse_path && c == '/' && (i + 1) < n && l[i + 1] == '*')
        {
          ++i;
          com = true;
          continue;
        }

        switch (s)
        {
        case start_hash:
          {
            if (c != '#')
              return false;

            s = start_keyword;
            break;
          }
        case start_keyword:
          {
            lex.clear ();
            s = parse_keyword;
            // Fall through.
          }
        case parse_keyword:
          {
            if (is_alpha (c))
            {
              lex += c;
              break;
            }

            if (lex != "include")
              return false;

            s = start_path;
            --i; // Re-parse the same character again.
            break;
          }
        case start_path:
          {
            if (c == '"')
            {
              path_end = '"';
              inc.type_ = include::quote;
            }
            else if (c == '<')
            {
              path_end = '>';
              inc.type_ = include::bracket;
            }
            else
              return false;

            lex.clear ();
            s = parse_path;
            break;
          }
        case parse_path:
          {
            if (c != path_end)
              lex += c;
            else
              s = parse_done;

            break;
          }
        default:
          {
            assert (false);
            break;
          }
        }

        if (s == parse_done)
          break;
      }

      if (s != parse_done)
        return false;

      inc.path_ = path (lex);
      return true;
    }

  private:
    bool
    is_alpha (char c) const
    {
      return isalpha (c, loc_);
    }

    bool
    is_space (char c) const
    {
      return isspace (c, loc_);
    }

  private:
    std::locale loc_;
  };
}

void
generate_include (context& ctx)
{
  include_map imap;

  traversal::unit unit;
  traversal::defines unit_defines;
  traversal::namespace_ ns;
  class_ c (ctx, imap);

  unit >> unit_defines >> ns;
  unit_defines >> c;

  traversal::defines ns_defines;

  ns >> ns_defines >> ns;
  ns_defines >> c;

  unit.dispatch (ctx.unit);

  // Add all the known include locations for each file in the map.
  //
  for (size_t i (0); i < line_table->used; ++i)
  {
    line_map const* m (line_table->maps + i);

    if (MAIN_FILE_P (m) || m->reason != LC_ENTER)
      continue;

    line_map const* i (INCLUDED_FROM (line_table, m));

    path f (m->to_file);
    f.complete ();
    f.normalize ();

    include_map::iterator it (imap.find (f));

    if (it != imap.end ())
      it->second[i] = include ();
  }

  //
  //
  file_map fmap;

  for (include_map::iterator i (imap.begin ()), e (imap.end ()); i != e; ++i)
  {
    /*
    cerr << endl
         << i->first << " included from" << endl;

    for (includes::iterator j (i->second.begin ()); j != i->second.end (); ++j)
    {
      line_map const* lm (j->first);
      cerr << '\t' << lm->to_file << ":" << LAST_SOURCE_LINE (lm) << endl;
    }
    */

    // First see if there is an include from the main file. If so, then
    // it is preferred over all others. Use the first one if there are
    // several.
    //
    line_map const* main_lm (0);
    include* main_inc (0);

    for (includes::iterator j (i->second.begin ()); j != i->second.end (); ++j)
    {
      line_map const* lm (j->first);

      if (MAIN_FILE_P (lm))
      {
        if (main_lm == 0 || LAST_SOURCE_LINE (main_lm) > LAST_SOURCE_LINE (lm))
        {
          main_lm = lm;
          main_inc = &j->second;
        }
      }
    }

    if (main_lm != 0)
    {
      path f (main_lm->to_file);
      f.complete ();
      f.normalize ();

      fmap[f][LAST_SOURCE_LINE (main_lm)] = main_inc;
      continue;
    }

    // Otherwise, add all entries.
    //
    for (includes::iterator j (i->second.begin ()); j != i->second.end (); ++j)
    {
      line_map const* lm (j->first);

      path f (lm->to_file);
      f.complete ();
      f.normalize ();

      fmap[f][LAST_SOURCE_LINE (lm)] = &j->second;
    }
  }

  //
  //
  include_parser ip;

  for (file_map::iterator i (fmap.begin ()), e (fmap.end ()); i != e; ++i)
  {
    ip.parse_file (i->first, i->second);
  }

  // Finally, output the include directives.
  //
  for (include_map::const_iterator i (imap.begin ()), e (imap.end ());
       i != e; ++i)
  {
    includes const& is (i->second);
    include const* inc (0);

    if (is.size () == 1)
    {
      inc = &is.begin ()->second;
    }
    else
    {
      include_set set;

      for (includes::const_iterator j (i->second.begin ());
           j != i->second.end (); ++j)
      {
        if (!j->second.path_.empty ())
          set.insert (&j->second);
      }

      assert (set.size () > 0);
      inc = *set.rbegin ();
    }

    path f (inc->path_.base ());
    f += ctx.options.odb_file_suffix ();
    f += ctx.options.hxx_suffix ();

    ctx.os << "#include " <<
      (inc->type_ == include::quote ? '"' : '<') << f <<
      (inc->type_ == include::quote ? '"' : '>') << endl
           << endl;
  }
}
