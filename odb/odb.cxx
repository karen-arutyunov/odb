// file      : odb/odb.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <errno.h>
#include <stdlib.h>    // getenv
#include <string.h>    // strerror
#include <unistd.h>    // stat, execvp
#include <sys/types.h> // stat
#include <sys/stat.h>  // stat

#include <string>
#include <vector>
#include <cstddef>     // size_t
#include <iostream>

using namespace std;

static string
plugin_path (string const& driver);

int
main (int argc, char* argv[])
{
  typedef vector<string> strings;

  string file;
  strings args;
  bool v (false);

  // Find the plugin. It should be in the same directory as the
  // driver.
  //
  string plugin (plugin_path (argv[0]));

  if (plugin.empty ())
  {
    cerr << argv[0] << ": error: unable to locate ODB plugin" << endl;
    cerr << argv[0] << ": info: make sure '" << argv[0] << ".so' is in "
         << "the same directory as '" << argv[0] << "'" << endl;
    return 1;
  }

  // The first argument points to the program name, which is
  // g++ by default.
  //
  args.push_back ("g++");

  // Default options.
  //
  args.push_back ("-x");
  args.push_back ("c++");
  args.push_back ("-S");
  args.push_back ("-fplugin=" + plugin);

  for (int i = 1; i < argc; ++i)
  {
    string a (argv[i]);
    size_t n (a.size ());

    // -v
    //
    if (a == "-v")
    {
      v = true;
      args.push_back (a);
    }
    // -x
    //
    else if (a == "-x")
    {
      if (++i == argc || argv[i][0] == '\0')
      {
        cerr << argv[0] << ": error: expected argument for the -x option"
             << endl;
        return 1;
      }

      a = argv[i];

      if (a[0] == '-')
        args.push_back (a);
      else
      {
        // This must be the g++ executable name. Update the first
        // argument with it.
        //
        args[0] = a;
      }
    }
    // -I
    //
    else if (n > 1 && a[0] == '-' && a[1] == 'I')
    {
      args.push_back (a);

      if (n == 2) // -I /path
      {
        if (++i == argc || argv[i][0] == '\0')
        {
          cerr << argv[0] << ": error: expected argument for the -I option"
               << endl;
          return 1;
        }

        args.push_back (argv[i]);
      }
    }
    // -D
    //
    else if (n > 1 && a[0] == '-' && a[1] == 'D')
    {
      args.push_back (a);

      if (n == 2) // -D macro
      {
        if (++i == argc || argv[i][0] == '\0')
        {
          cerr << argv[0] << ": error: expected argument for the -D option"
               << endl;
          return 1;
        }

        args.push_back (argv[i]);
      }
    }
    // -U
    //
    else if (n > 1 && a[0] == '-' && a[1] == 'U')
    {
      args.push_back (a);

      if (n == 2) // -U macro
      {
        if (++i == argc || argv[i][0] == '\0')
        {
          cerr << argv[0] << ": error: expected argument for the -U option"
               << endl;
          return 1;
        }

        args.push_back (argv[i]);
      }
    }
    // Everything else except the last argument (file to compile) is passed
    // to the plugin.
    //
    else
    {
      if (n > 1 && a[0] == '-')
      {
        string k, v;

        if (n > 2 && a[1] == '-')
          k = string (a, 2); // long format
        else
          k = string (a, 1); // short format

        // If the next argument is not the last, then we may have a
        // value.
        //
        if (i + 2 < argc)
        {
          a = argv[i + 1];
          n = a.size ();

          if (n > 1 && a[0] != '-')
          {
            v = a;
            ++i;
          }
        }

        string o ("-fplugin-arg-odb-");
        o += k;

        if (!v.empty ())
        {
          o += '=';
          o += v;
        }

        args.push_back (o);
      }
      else
      {
        if (file.empty ())
          file = a;
        else
        {
          cerr << argv[0] << ": error: second input file specified: '"
               << a << "'" << endl;
          return 1;
        }
      }
    }
  }

  if (file.empty ())
  {
    cerr << argv[0] << ": error: input file expected" << endl;
    return 1;
  }

  args.push_back (file);

  // Create an execvp-compatible argument array.
  //
  vector<char const*> exec_args;

  for (strings::const_iterator i (args.begin ()), e (args.end ());
       i != e; ++i)
  {
    exec_args.push_back (i->c_str ());

    if (v)
      cerr << *i << ' ';
  }

  if (v)
    cerr << endl;

  exec_args.push_back (0);

  if (execvp (exec_args[0], const_cast<char**> (&exec_args[0])) < 0)
  {
    cerr << exec_args[0] << ": error: " << strerror (errno) << endl;
    return 1;
  }
}

static string
plugin_path (string const& drv)
{
  size_t p (drv.rfind ('/'));

  if (p != string::npos)
    return drv + ".so";

  // Search the PATH environment variable.
  //
  string paths;

  // If there is no PATH in environment then the default search
  // path is the current directory.
  //
  if (char const* s = getenv ("PATH"))
    paths = s;
  else
    paths = ":";

  for (size_t b (0), e (paths.find (':')); b != string::npos;)
  {
    string p (paths, b, e - b);

    // Empty path (i.e., a double colon or a colon at the beginning
    // or end of PATH) means search in the current dirrectory.
    //
    if (p.empty ())
      p = ".";

    string dp (p + (p[p.size () - 1] == '/' ? "" : "/") + drv);

    // Just check that the file exist without checking for
    // permissions, etc.
    //
    struct stat info;
    if (stat (dp.c_str (), &info) == 0 && S_ISREG (info.st_mode))
      return dp + ".so";

    if (e == string::npos)
      b = e;
    else
    {
      b = e + 1;
      e = paths.find (':', b);
    }
  }

  return "";
}
