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

#include <odb/version.hxx>
#include <odb/options.hxx>

using namespace std;

static string
plugin_path (string const& driver);

int
main (int argc, char* argv[])
{
  typedef vector<string> strings;

  ostream& e (cerr);

  // Find the plugin. It should be in the same directory as the
  // driver.
  //
  string plugin (plugin_path (argv[0]));

  if (plugin.empty ())
  {
    e << argv[0] << ": error: unable to locate ODB GCC plugin" << endl;
    e << argv[0] << ": info: make sure '" << argv[0] << ".so' is in "
      << "the same directory as '" << argv[0] << "'" << endl;
    return 1;
  }

  strings args, plugin_args;
  bool v (false);

  // The first argument points to the program name, which is
  // g++ by default.
  //
  args.push_back ("g++");

  // Default options.
  //
  args.push_back ("-x");
  args.push_back ("c++");
  args.push_back ("-S");
  args.push_back ("-DODB_COMPILER");
  args.push_back ("-fplugin=" + plugin);

  // Parse driver options.
  //
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
        e << argv[0] << ": error: expected argument for the -x option" << endl;
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
          e << argv[0] << ": error: expected argument for the -I option"
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
          e << argv[0] << ": error: expected argument for the -D option"
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
          e << argv[0] << ": error: expected argument for the -U option"
            << endl;
          return 1;
        }

        args.push_back (argv[i]);
      }
    }
    // Store everything else in a list so that we can parse it with the
    // cli parser. This is the only reliable way to find out where the
    // options end.
    //
    else
      plugin_args.push_back (a);
  }

  // Parse plugin options.
  //
  try
  {
    vector<char*> av;
    av.push_back (argv[0]);

    for (strings::iterator i (plugin_args.begin ()), end (plugin_args.end ());
         i != end; ++i)
    {
      av.push_back (const_cast<char*> (i->c_str ()));
    }

    int ac (static_cast<int> (av.size ()));
    cli::argv_file_scanner scan (ac, &av[0], "--options-file");

    options ops (scan);

    // Handle --version.
    //
    if (ops.version ())
    {
      e << "CodeSynthesis ODB object persistence compiler for C++ " <<
        ODB_COMPILER_VERSION_STR << endl
        << "Copyright (C) 2009-2010 Code Synthesis Tools CC" << endl;

      if (ops.proprietary_license ())
      {
        e << "The compiler was invoked in the Proprietary License mode. You "
          << "should have\nreceived a proprietary license from Code Synthesis "
          << "Tools CC that entitles\nyou to use it in this mode." << endl;
      }
      else
      {
        e << "This is free software; see the source for copying conditions. "
          << "There is NO\nwarranty; not even for MERCHANTABILITY or FITNESS "
          << "FOR A PARTICULAR PURPOSE." << endl;
      }

      return 0;
    }

    // Handle --help.
    //
    if (ops.help ())
    {
      e << "Usage: " << argv[0] << " [options] file [file ...]"
        << endl
        << "Options:" << endl;

      options::print_usage (e);
      return 0;
    }

    size_t end (scan.end () - 1); // We have one less in plugin_args.

    if (end == plugin_args.size ())
    {
      e << argv[0] << ": error: input file expected" << endl;
      return 1;
    }

    // Encode plugin options.
    //
    for (size_t i (0); i < end; ++i)
    {
      string k, v;
      string a (plugin_args[i]);

      if (a == "--")
      {
        // Ignore the option seperator since GCC doesn't understand it.
        //
        continue;
      }

      if (a.size () > 2)
        k = string (a, 2); // long format
      else
        k = string (a, 1); // short format

      // If there are more arguments then we may have a value.
      //
      if (i + 1 < end)
      {
        a = plugin_args[i + 1];
        if (a.size () > 1 && a[0] != '-')
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

    // Copy over arguments.
    //
    args.insert (args.end (), plugin_args.begin () + end, plugin_args.end ());
  }
  catch (cli::exception const& ex)
  {
    e << ex << endl;
    return 1;
  }

  // Create an execvp-compatible argument array.
  //
  vector<char const*> exec_args;

  for (strings::const_iterator i (args.begin ()), end (args.end ());
       i != end; ++i)
  {
    exec_args.push_back (i->c_str ());

    if (v)
      e << *i << ' ';
  }

  if (v)
    e << endl;

  exec_args.push_back (0);

  if (execvp (exec_args[0], const_cast<char**> (&exec_args[0])) < 0)
  {
    e << exec_args[0] << ": error: " << strerror (errno) << endl;
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
    string p (paths, b, e != string::npos ? e - b : e);

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
