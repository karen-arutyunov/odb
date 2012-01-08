// file      : odb/include.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <set>
#include <map>
#include <locale>
#include <cassert>
#include <fstream>
#include <sstream>

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/generate.hxx>

#include <iostream>

using namespace std;
using semantics::path;

namespace
{
  struct include_directive
  {
    enum type { quote, bracket };

    type type_;
    path path_;
  };

  typedef std::map<line_map const*, include_directive> includes;
  typedef std::map<path, includes> include_map;

  // Map of files to the lines which contain include directives
  // that we are interested in.
  //
  typedef std::map<size_t, include_directive*> include_lines;
  typedef std::map<string, include_lines> file_map;

  // Set of include directives sorted in the preference order.
  //
  struct include_comparator
  {
    bool
    operator() (include_directive const* x, include_directive const* y) const
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

  typedef
  std::multiset<include_directive const*, include_comparator>
  include_set;


  struct class_: traversal::class_, context
  {
    class_ (include_map& map)
        : map_ (map)
    {
    }

    virtual void
    traverse (type& c)
    {
      // We only generate things for objects and composite value types. In
      // particular, we don't care about views since they cannot be used in
      // definitions of other views, objects, or composite values.
      //
      if (!(object (c) || composite (c)))
        return;

      // Not interested in classes that we are generating.
      //
      // If this is a class template instantiation, then get the file
      // corresponding to the pragma, not the instantiation itself,
      // since that's where we are generation the code for this class.
      // While at it, also get the location.
      //
      using semantics::path;

      path f;
      location_t l;

      if (c.is_a<semantics::class_instantiation> ())
      {
        l = c.get<location_t> ("location");
        f = path (LOCATION_FILE (l));
      }
      else
      {
        f = c.file ();
        tree decl (TYPE_NAME (c.tree_node ()));
        l = DECL_SOURCE_LOCATION (decl);
      }

      if (f == unit.file ())
        return;

      // This is a persistent object or composite value type declared in
      // another header file. Include its -odb header.
      //
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
            map_[f][lm] = include_directive ();
        }
      }
    }

  private:
    include_map& map_;
  };

  class include_parser
  {
  public:
    include_parser (options const& options)
        : loc_ ("C"), options_ (options)
    {
    }

    void
    parse_file (string const& file, include_lines& lines)
    {
      string f (file);
      size_t n (f.size ());

      // Check if we have a synthesized prologue/epilogue fragment.
      //
      if (n != 0 && f[0] == '<' && f[n - 1] == '>')
      {
        size_t p (f.rfind ('-'));

        if (p != string::npos)
        {
          string name (f, 1, p - 1);

          if (name == "odb-prologue" || name == "odb-epilogue")
          {
            // Extract the fragment number.
            //
            {
              istringstream istr (string (f, p + 1));
              istr >> n;
            }

            n--; // Prologues/epilogues are counted from 1.

            stringstream ss;
            f.clear ();

            // We don't need the #line part.
            //
            if (name == "odb-prologue")
            {
              size_t size (options_.odb_prologue ().size ());

              if (n < size)
                ss << options_.odb_prologue ()[n];
              else
                f = options_.odb_prologue_file ()[n - size];
            }
            else
            {
              size_t size (options_.odb_epilogue ().size ());

              if (n < size)
                ss << options_.odb_epilogue ()[n];
              else
                f = options_.odb_epilogue_file ()[n - size];
            }

            if (f.empty ())
            {
              parse_stream (ss, file, lines);
              return;
            }
            // Otherwise use the code below to parse the file.
          }
        }
      }

      ifstream is (f.c_str ());

      if (!is.is_open ())
      {
        cerr << "error: unable to open '" << f << "' in read mode" << endl;
        throw operation_failed ();
      }

      parse_stream (is, f, lines);
    }

    void
    parse_stream (istream& is, string const& name, include_lines& lines)
    {
      typedef char_traits<char>::int_type int_type;

      size_t lmax (lines.rbegin ()->first);

      string line;
      bool bslash (false);
      size_t lb (1), le (1);
      bool eof (false);

      for (int_type c (is.get ()); !eof; c = is.get ())
      {
        if (is.fail ())
        {
          if (is.eof ())
          {
            // If we are still in the range, treat this as the last newline.
            //
            c = '\n';
            eof = true;
          }
          else
            break; // Some other failure -- bail out.
        }

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
                  cerr << name << ":" << lb << ":1: error: "
                       << "unable to parse #include directive" << endl;
                  throw operation_failed ();
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
        cerr << "error: input error while reading '" << name << "'" << endl;
        throw operation_failed ();
      }
    }

  private:
    bool
    parse_line (string const& l, include_directive& inc)
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
      char path_end ('\0');
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
              inc.type_ = include_directive::quote;
            }
            else if (c == '<')
            {
              path_end = '>';
              inc.type_ = include_directive::bracket;
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
    options const& options_;
  };
}

namespace include
{
  void
  generate ()
  {
    context ctx;
    include_map imap;

    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (true);
    traversal::namespace_ ns;
    class_ c (imap);

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (true);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

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
        it->second[i] = include_directive ();
    }

    //
    //
    file_map fmap;

    for (include_map::iterator i (imap.begin ()), e (imap.end ()); i != e; ++i)
    {
      /*
      cerr << endl
           << i->first << " included from" << endl;

      for (includes::iterator j (i->second.begin ());
           j != i->second.end (); ++j)
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
      include_directive* main_inc (0);

      for (includes::iterator j (i->second.begin ());
           j != i->second.end (); ++j)
      {
        line_map const* lm (j->first);

        if (MAIN_FILE_P (lm))
        {
          if (main_lm == 0 ||
              LAST_SOURCE_LINE (main_lm) > LAST_SOURCE_LINE (lm))
          {
            main_lm = lm;
            main_inc = &j->second;
          }
        }
      }

      if (main_lm != 0)
      {
        string f (main_lm->to_file);
        size_t n (f.size ());

        // Check if this is a synthesized fragment.
        //
        if (!(n != 0 && f[0] == '<' && f[n - 1] == '>'))
        {
          path p (f);
          p.complete ();
          p.normalize ();
          f = p.string ();
        }

        fmap[f][LAST_SOURCE_LINE (main_lm)] = main_inc;
        continue;
      }

      // Otherwise, add all entries.
      //
      for (includes::iterator j (i->second.begin ());
           j != i->second.end (); ++j)
      {
        line_map const* lm (j->first);

        string f (lm->to_file);
        size_t n (f.size ());

        // Check if this is a synthesized fragment.
        //
        if (!(n != 0 && f[0] == '<' && f[n - 1] == '>'))
        {
          path p (f);
          p.complete ();
          p.normalize ();
          f = p.string ();
        }

        fmap[f][LAST_SOURCE_LINE (lm)] = &j->second;
      }
    }

    //
    //
    include_parser ip (ctx.options);

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
      include_directive const* inc (0);

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

      char o (inc->type_ == include_directive::quote ? '"' : '<');
      ctx.os << "#include " << ctx.process_include_path (
        f.string (), false, o) << endl
             << endl;
    }
  }
}
