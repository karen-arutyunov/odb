// file      : odb/odb.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <errno.h>
#include <stdlib.h>    // getenv
#include <string.h>    // strerror
#include <unistd.h>    // stat, execvp
#include <sys/types.h> // stat
#include <sys/stat.h>  // stat

#ifdef _WIN32
#  include <process.h> // _spawnvp
#endif

#include <string>
#include <vector>
#include <cstddef>     // size_t
#include <iostream>

#include <cutl/fs/path.hxx>

#include <odb/version.hxx>
#include <odb/options.hxx>

#ifdef HAVE_CONFIG_H
#  include <odb/config.h>
#endif

using namespace std;
using cutl::fs::path;

static path
driver_path (path const& driver);

static path
plugin_path (path const& driver);

static char const* const db_macro[] =
{
  "-DODB_DATABASE_MYSQL",
  "-DODB_DATABASE_TRACER"
};

int
main (int argc, char* argv[])
{
  typedef vector<string> strings;

  ostream& e (cerr);

  // Find the plugin. It should be in the same directory as the
  // driver.
  //
#ifndef STATIC_PLUGIN
  path plugin (plugin_path (path (argv[0])));
#else
  // Use a dummy name if the plugin is linked into the compiler.
  //
  path plugin ("odb");
#endif

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
#ifdef GXX_NAME
  path gxx (GXX_NAME);

  // If the g++ name is a relative path (starts with '.'), then use
  // our own path as base.
  //
  if (!gxx.empty () && gxx.string ()[0] == '.')
  {
    path dp (driver_path (path (argv[0])));
    path d (dp.directory ());

    if (!d.empty ())
      gxx = d / gxx;
  }

  args.push_back (gxx.string ());
#else
  args.push_back ("g++");
#endif

  // Default options.
  //
  args.push_back ("-x");
  args.push_back ("c++");
  args.push_back ("-S");
  args.push_back ("-Wunknown-pragmas");
  args.push_back ("-fplugin=" + plugin.string ());

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
      e << "ODB object-relational mapping (ORM) compiler for C++ "
        ODB_COMPILER_VERSION_STR << endl
        << "Copyright (C) 2009-2010 Code Synthesis Tools CC" << endl;

      e << "This is free software; see the source for copying conditions. "
        << "There is NO\nwarranty; not even for MERCHANTABILITY or FITNESS "
        << "FOR A PARTICULAR PURPOSE." << endl;

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

    // Check that required options were specifed.
    //
    if (!ops.database_specified ())
    {
      e << argv[0] << ": error: no database specified with the --database "
        << "option" << endl;
      return 1;
    }

    size_t end (scan.end () - 1); // We have one less in plugin_args.

    if (end == plugin_args.size ())
    {
      e << argv[0] << ": error: input file expected" << endl;
      return 1;
    }

    // Add ODB macros.
    //
    args.push_back ("-DODB_COMPILER");
    args.push_back (db_macro[ops.database ()]);

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

        // On Windows we need to protect values with spaces using quotes.
        // Since there could be actual quotes in the value, we need to
        // escape them.
        //
#ifdef _WIN32
        {
          string t ("\"");
          for (size_t i (0); i < v.size (); ++i)
          {
            if (v[i] == '"')
              t += "\\\"";
            else
              t += v[i];
          }
          t += '"';
          v = t;
        }
#endif
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

#ifdef _WIN32
  intptr_t r (_spawnvp (_P_WAIT, exec_args[0], &exec_args[0]));

  if (r == (intptr_t)(-1))
  {
    e << exec_args[0] << ": error: " << strerror (errno) << endl;
    return 1;
  }

  return r == 0 ? 0 : 1;
#else
  if (execvp (exec_args[0], const_cast<char**> (&exec_args[0])) < 0)
  {
    e << exec_args[0] << ": error: " << strerror (errno) << endl;
    return 1;
  }
#endif
}

//
// Path manipulation.
//

static path
driver_path (path const& drv)
{
  typedef path::traits traits;

  if (!drv.directory ().empty ())
    return drv;

  // Search the PATH environment variable.
  //
  string paths;

  // If there is no PATH in environment then the default search
  // path is the current directory.
  //
  if (char const* s = getenv ("PATH"))
    paths = s;
  else
    paths = traits::path_separator;

  // On Windows also check the current directory.
  //
#ifdef _WIN32
  paths += traits::path_separator;
#endif

  struct stat info;

  for (size_t b (0), e (paths.find (traits::path_separator));
       b != string::npos;)
  {
    path p (string (paths, b, e != string::npos ? e - b : e));

    // Empty path (i.e., a double colon or a colon at the beginning
    // or end of PATH) means search in the current dirrectory.
    //
    if (p.empty ())
      p = path (".");

    path dp (p / drv);

    // Just check that the file exist without checking for
    // permissions, etc.
    //
    if (stat (dp.string ().c_str (), &info) == 0 && S_ISREG (info.st_mode))
      return dp;

    // On Windows also try the path with the .exe extension.
    //
#ifdef _WIN32
    dp += ".exe";

    if (stat (dp.string ().c_str (), &info) == 0 && S_ISREG (info.st_mode))
      return dp;
#endif

    if (e == string::npos)
      b = e;
    else
    {
      b = e + 1;
      e = paths.find (traits::path_separator, b);
    }
  }

  return path ();
}

static path
plugin_path (path const& drv)
{
  path dp (driver_path (drv));

  if (!dp.empty ())
  {
    // If the driver name starts with 'lt-', then we are running through
    // the libtool script. Strip this prefix -- the shared object should
    // be in the same directory.
    //
    {
      string n (dp.leaf ().string ());

      if (n.size () > 3 && n[0] == 'l' && n[1] == 't' && n[2] == '-')
        dp = dp.directory () / path (string (n, 3, string::npos));
    }

    struct stat info;

    path so (dp + ".so");
    if (stat (so.string ().c_str (), &info) == 0)
      return so;

    path la (dp + ".la");
    if (stat (la.string ().c_str (), &info) == 0)
    {
      so = la.directory () / path (".libs") / dp.leaf () + ".so";

      if (stat (so.string ().c_str (), &info) == 0)
        return so;
    }
  }

  return path ();
}
