// file      : odb/odb.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <errno.h>
#include <stdlib.h>    // getenv, setenv
#include <string.h>    // strerror, memset
#include <unistd.h>    // stat, close
#include <sys/types.h> // stat
#include <sys/stat.h>  // stat

// Process.
//
#ifndef _WIN32
#  include <unistd.h>    // execvp, fork, dup2, pipe, {STDIN,STDERR}_FILENO
#  include <sys/types.h> // waitpid
#  include <sys/wait.h>  // waitpid
#else
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>   // CreatePipe, CreateProcess
#  include <io.h>        // _open_osfhandle
#endif

#include <string>
#include <vector>
#include <cstddef>     // size_t
#include <sstream>
#include <fstream>
#include <iostream>
#include <ext/stdio_filebuf.h>

#include <cutl/fs/path.hxx>

#include <odb/version.hxx>
#include <odb/options.hxx>

#ifdef HAVE_CONFIG_H
#  include <odb/config.h>
#endif

using namespace std;
using cutl::fs::path;
using cutl::fs::invalid_path;

typedef vector<string> strings;
typedef vector<path> paths;

//
// Path manipulation.
//

// Escape backslashes in the path.
//
static string
escape_path (path const& p);

// Search the PATH environment variable for the file.
//
static path
path_search (path const&);

// Driver path with the directory part (search PATH).
//
static path
driver_path (path const& driver);

static path
plugin_path (path const& driver);

//
// Process manipulation.
//
struct process_info
{
#ifndef _WIN32
  pid_t id;
#else
  HANDLE id;
#endif

  int out_fd;
  int in_fd;
};

struct process_failure {};

// Start another process using the specified command line. Connect the
// newly created process' stdin to out_fd. Also if connect_out is true,
// connect the created process' stdout and stderr to in_fd. Issue
// diagnostics and throw process_failure if anything goes wrong. The
// name argument is the name of the current process for diagnostics.
//
static process_info
start_process (char const* args[], char const* name, bool connect_out = false);

// Wait for the process to terminate. Return true if the process terminated
// normally and with the zero exit status. Issue diagnostics and throw
// process_failure if anything goes wrong. The name argument is the name
// of the current process for diagnostics.
//
static bool
wait_process (process_info, char const* name);

//
//
static string
encode_plugin_option (string const& k, string const& v);

// Extract header search paths from GCC's -v output. May throw the
// profile_failure, process_failure and invalid_path exceptions. Name
// is the program name (argv[0]) for diagnostics.
//
struct profile_failure {};

static paths
profile_paths (strings const& args, char const* name);

// Search for the profile options file.
//
struct profile_data
{
  profile_data (paths const& p, char const* n): search_paths (p), name (n) {}

  paths const& search_paths;
  char const* name;
};

static string
profile_search (char const* profile, void* arg);

static char const* const db_macro[] =
{
  "-DODB_DATABASE_MYSQL",
  "-DODB_DATABASE_TRACER"
};

int
main (int argc, char* argv[])
{
  ostream& e (cerr);

  try
  {
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

    if (gxx.empty ())
    {
      e << argv[0] << ": error: embedded g++ compile name is empty" << endl;
      return 1;
    }

    // If the g++ name is a relative path (starts with '.'), then use
    // our own path as base.
    //
    if (gxx.string ()[0] == '.')
    {
      path dp (driver_path (path (argv[0])));
      path d (dp.directory ());

      if (!d.empty ())
        gxx = d / gxx;
    }

    args.push_back (gxx.string ());

    // Also modify LD_LIBRARY_PATH to include the lib path.
    //
#ifndef _WIN32
    {
      string ld_paths;

      if (char const* s = getenv ("LD_LIBRARY_PATH"))
        ld_paths = s;

      path d (gxx.directory ());

      if (!d.empty ())
      {
        d.complete ();
        d /= path ("..") / path ("lib");

        if (ld_paths.empty ())
          ld_paths = d.string ();
        else
          ld_paths = d.string () + path::traits::path_separator + ld_paths;

        if (setenv ("LD_LIBRARY_PATH", ld_paths.c_str (), 1) != 0)
        {
          e << argv[0] << ": error: unable to update environment" << endl;
          return 1;
        }
      }
    }
#endif // _WIN32

#else
    args.push_back ("g++");
#endif // GXX_NAME

    // Default options.
    //
    args.push_back ("-x");
    args.push_back ("c++");
    args.push_back ("-S");
    args.push_back ("-Wunknown-pragmas");
    args.push_back ("-fplugin=" + plugin.string ());

    // Parse driver options.
    //
    bool first_x (true);

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

        if (first_x)
        {
          first_x = false;

          // If it doesn't start with '-', then it must be the g++
          // executable name. Update the first argument with it.
          //
          if (a[0] != '-')
            args[0] = a;
          else
            args.push_back (a);
        }
        else
          args.push_back (a);
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

    // Obtain profile (-I) search paths.
    //
    paths prof_paths (profile_paths (args, argv[0]));

    if (v)
    {
      e << "Profile search paths:" << endl;

      for (paths::const_iterator i (prof_paths.begin ());
           i != prof_paths.end (); ++i)
        e << " " << *i << endl;
    }

    // Parse plugin options.
    //
    vector<char*> av;
    av.push_back (argv[0]);

    for (strings::iterator i (plugin_args.begin ()), end (plugin_args.end ());
         i != end; ++i)
    {
      av.push_back (const_cast<char*> (i->c_str ()));
    }

    int ac (static_cast<int> (av.size ()));

    profile_data pd (prof_paths, argv[0]);
    cli::argv_file_scanner::option_info oi[3];
    oi[0].option = "--options-file";
    oi[0].search_func = 0;
    oi[1].option = "-p";
    oi[1].search_func = &profile_search;
    oi[1].arg = &pd;
    oi[2].option = "--profile";
    oi[2].search_func = &profile_search;
    oi[2].arg = &pd;

    cli::argv_file_scanner scan (ac, &av[0], oi, 3);

    options ops (scan);

    // Handle --version.
    //
    if (ops.version ())
    {
      e << "ODB object-relational mapping (ORM) compiler for C++ "
        ODB_COMPILER_VERSION_STR << endl
        << "Copyright (c) 2009-2011 Code Synthesis Tools CC" << endl;

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

    {
      ostringstream ostr;
      ostr << ODB_COMPILER_VERSION;
      args.push_back ("-DODB_COMPILER_VERSION=" + ostr.str ());
    }

    args.push_back (db_macro[ops.database ()]);

    // Encode plugin options.
    //
    cli::options const& desc (options::description ());
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

      cli::options::const_iterator it (desc.find (a));

      if (it == desc.end ())
      {
        e << argv[0] << ": ice: unexpected option '" << a << "'" << endl;
        return 1;
      }

      if (a.size () > 2 && a[0] == '-' && a[1] == '-')
        k = string (a, 2); // long format
      else
        k = string (a, 1); // short format

      // If there are more arguments then we may have a value.
      //
      if (!it->flag ())
      {
        if (i + 1 == end)
        {
          e << argv[0] << ": ice: expected argument for '" << a << "'" << endl;
          return 1;
        }

        v = plugin_args[++i];
      }

      args.push_back (encode_plugin_option (k, v));
    }

    // Reserve space for and remember the position of the --svc-file
    // option.
    //
    size_t svc_file_pos (args.size ());
    args.push_back ("");

    // Create an execvp-compatible argument array.
    //
    typedef vector<char const*> cstrings;
    cstrings exec_args;

    for (strings::const_iterator i (args.begin ()), end (args.end ());
         i != end; ++i)
    {
      exec_args.push_back (i->c_str ());
    }

    exec_args.push_back ("-"); // Compile stdin.
    exec_args.push_back (0);

    // Iterate over the input files and compile each of them.
    //
    for (; end < plugin_args.size (); ++end)
    {
      path input (plugin_args[end]);

      // Set the --svc-file option.
      //
      args[svc_file_pos] = encode_plugin_option ("svc-file", input.string ());
      exec_args[svc_file_pos] = args[svc_file_pos].c_str ();

      //
      //
      ifstream ifs (input.string ().c_str (), ios_base::in | ios_base::binary);

      if (!ifs.is_open ())
      {
        cerr << input << ": error: unable to open in read mode" << endl;
        return 1;
      }

      if (v)
      {
        e << "Compiling " << input << endl;
        for (cstrings::const_iterator i (exec_args.begin ());
             i != exec_args.end (); ++i)
        {
          if (*i != 0)
            e << *i << (*(i + 1) != 0 ? ' ' : '\n');
        }
      }

      process_info pi (start_process (&exec_args[0], argv[0]));

      {
        __gnu_cxx::stdio_filebuf<char> fb (
          pi.out_fd, ios_base::out | ios_base::binary);
        ostream os (&fb);

        // Add the standard prologue.
        //
        // os << "#line 1 \"<standard-odb-prologue>\"" << endl;

        // Add custom prologue if any.
        //
        strings const& pro (ops.odb_prologue ());
        for (size_t i (0); i < pro.size (); ++i)
        {
          os << "#line 1 \"<odb-prologue-" << i + 1 << ">\"" << endl
             << pro[i]
             << endl;
        }

        // Write the synthesized translation unit to stdout.
        //
        os << "#line 1 \"" << escape_path (input) << "\"" << endl;

        if (!(os << ifs.rdbuf ()))
        {
          e << input << ": error: io failure" << endl;
          wait_process (pi, argv[0]);
          return 1;
        }

        // Add a new line in case the input file doesn't end with one.
        //
        os << endl;

        // Add custom epilogue if any.
        //
        strings const& epi (ops.odb_epilogue ());
        for (size_t i (0); i < epi.size (); ++i)
        {
          os << "#line 1 \"<odb-epilogue-" << i + 1 << ">\"" << endl
             << epi[i]
             << endl;
        }

        // Add the standard epilogue at the end so that we see all
        // the declarations.
        //
        os << "#line 1 \"<standard-odb-epilogue>\"" << endl;

        // Includes for standard smart pointers. The Boost TR1 header
        // may or may not delegate to the GCC implementation. In either
        // case, the necessary declarations will be provided so we don't
        // need to do anything.
        //
        os << "#include <memory>" << endl
           << "#ifndef BOOST_TR1_MEMORY_HPP_INCLUDED" << endl
           << "#  include <tr1/memory>" << endl
           << "#endif" << endl;

        // Standard pointer traits.
        //
        os << "#include <odb/pointer-traits.hxx>" << endl
           << "#include <odb/tr1/pointer-traits.hxx>" << endl;

        // Standard container traits.
        //
        os << "#include <odb/container-traits.hxx>" << endl;
      }

      if (!wait_process (pi, argv[0]))
        return 1;
    }
  }
  catch (profile_failure const&)
  {
    // Diagnostics has already been issued.
    //
    return 1;
  }
  catch (process_failure const&)
  {
    // Diagnostics has already been issued.
    //
    return 1;
  }
  catch (invalid_path const& ex)
  {
    e << argv[0] << ": error: invalid path '" << ex.path () << "'" << endl;
    return 1;
  }
  catch (cli::exception const& ex)
  {
    e << ex << endl;
    return 1;
  }

}

static string
encode_plugin_option (string const& k, string const& cv)
{
  string o ("-fplugin-arg-odb-"), v (cv);
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

  return o;
}

static paths
profile_paths (strings const& sargs, char const* name)
{
  // Copy some of the arguments from the passed list. We only need
  // the g++ executable and the -I options.
  //
  strings args;

  args.push_back (sargs[0]);
  args.push_back ("-v");
  args.push_back ("-x");
  args.push_back ("c++");
  args.push_back ("-E");
  args.push_back ("-P");

  for (strings::const_iterator i (++sargs.begin ()), end (sargs.end ());
       i != end; ++i)
  {
    string const& a (*i);

    if (a.size () > 1 && a[0] == '-' && a[1] == 'I')
    {
      args.push_back (a);

      if (a.size () == 2) // -I /path
      {
        ++i;
        args.push_back (*i);
      }
    }
  }

  // Create an execvp-compatible argument array.
  //
  vector<char const*> exec_args;

  for (strings::const_iterator i (args.begin ()), end (args.end ());
       i != end; ++i)
  {
    exec_args.push_back (i->c_str ());
  }

  exec_args.push_back ("-"); // Compile stdin.
  exec_args.push_back (0);

  process_info pi (start_process (&exec_args[0], name, true));
  close (pi.out_fd); // Preprocess empty file.

  // Read and parse the output.
  //
  paths r;
  {
    __gnu_cxx::stdio_filebuf<char> fb (pi.in_fd, ios_base::in);
    istream is (&fb);

    enum
    {
      read_prefix,
      read_path,
      read_suffix
    } state = read_prefix;

    while (!is.eof ())
    {
      string line;
      getline (is, line);

      if (is.fail () && !is.eof ())
      {
        cerr << name << ": error: "
             << "io failure while parsing profile paths" << endl;
        throw profile_failure ();
      }

      switch (state)
      {
      case read_prefix:
        {
          if (line == "#include <...> search starts here:")
            state = read_path;
          break;
        }
      case read_path:
        {
          if (line == "End of search list.")
            state = read_suffix;
          else
            // Paths are indented with a space.
            //
            r.push_back (path (string (line, 1)));

          break;
        }
      case read_suffix:
        {
          // Keep reading until eof to make sure the process is not
          // blocked.
          break;
        }
      }
    }

    if (state != read_suffix)
    {
      cerr << name << ": error: unable to parse profile paths" << endl;
      throw profile_failure ();
    }
  }

  wait_process (pi, name);
  return r;
}

static string
profile_search (char const* prof, void* arg)
{
  profile_data* pd (static_cast<profile_data*> (arg));
  paths const& ps (pd->search_paths);

  path p (prof), odb ("odb"), r;
  p.normalize (); // Convert '/' to the canonical path separator form.
  p += ".options";

  struct stat info;

  for (paths::const_iterator i (ps.begin ()), end (ps.end ()); i != end; ++i)
  {
    // First check in the search directory itself and then try the odb/
    // subdirectory.
    //
    r = *i / p;

    // Just check that the file exist without checking for permissions, etc.
    //
    if (stat (r.string ().c_str (), &info) == 0 && S_ISREG (info.st_mode))
      return r.string ();

    r = *i / odb / p;

    if (stat (r.string ().c_str (), &info) == 0 && S_ISREG (info.st_mode))
      return r.string ();
  }

  cerr << pd->name << ": error: unable to locate options file for profile '"
       << prof << "'" << endl;
  throw profile_failure ();
}

//
// Path manipulation.
//

static string
escape_path (path const& p)
{
  string r;
  string const& s (p.string ());

  for (size_t i (0); i < s.size (); ++i)
  {
    if (s[i] == '\\')
      r += "\\\\";
    else
      r += s[i];
  }

  return r;
}

static path
path_search (path const& f)
{
  typedef path::traits traits;

  // If there is a directory component in the file, then the PATH
  // search does not apply.
  //
  if (!f.directory ().empty ())
    return f;

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

    path dp (p / f);

    // Just check that the file exist without checking for permissions, etc.
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
driver_path (path const& drv)
{
  return drv.directory ().empty () ? path_search (drv) : drv;
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

//
// Process manipulation.
//

#ifndef _WIN32

static process_info
start_process (char const* args[], char const* name, bool out)
{
  int out_fd[2];
  int in_fd[2];

  if (pipe (out_fd) == -1 || (out && pipe (in_fd) == -1))
  {
    char const* err (strerror (errno));
    cerr << name << ": error: " <<  err << endl;
    throw process_failure ();
  }

  pid_t pid (fork ());

  if (pid == -1)
  {
    char const* err (strerror (errno));
    cerr << name << ": error: " <<  err << endl;
    throw process_failure ();
  }

  if (pid == 0)
  {
    // Child. Close the write end of the pipe and duplicate the read end
    // to stdin. Then close the original read end descriptors.
    //
    if (close (out_fd[1]) == -1 ||
        dup2 (out_fd[0], STDIN_FILENO) == -1 ||
        close (out_fd[0]) == -1)
    {
      char const* err (strerror (errno));
      cerr << name << ": error: " <<  err << endl;
      throw process_failure ();
    }

    // Do the same for the out if requested.
    //
    if (out)
    {
      if (close (in_fd[0]) == -1 ||
          dup2 (in_fd[1], STDOUT_FILENO) == -1 ||
          dup2 (in_fd[1], STDERR_FILENO) == -1 ||
          close (in_fd[1]) == -1)
      {
        char const* err (strerror (errno));
        cerr << name << ": error: " <<  err << endl;
        throw process_failure ();
      }
    }

    if (execvp (args[0], const_cast<char**> (&args[0])) == -1)
    {
      char const* err (strerror (errno));
      cerr << args[0] << ": error: " << err << endl;
      throw process_failure ();
    }
  }
  else
  {
    // Parent. Close the other ends of the pipes.
    //
    if (close (out_fd[0]) == -1 || (out && close (in_fd[1]) == -1))
    {
      char const* err (strerror (errno));
      cerr << name << ": error: " <<  err << endl;
      throw process_failure ();
    }
  }

  process_info r;
  r.id = pid;
  r.out_fd = out_fd[1];
  r.in_fd = out ? in_fd[0] : 0;
  return r;
}

static bool
wait_process (process_info pi, char const* name)
{
  int status;

  if (waitpid (pi.id, &status, 0) == -1)
  {
    char const* err (strerror (errno));
    cerr << name << ": error: " <<  err << endl;
    throw process_failure ();
  }

  return WIFEXITED (status) && WEXITSTATUS (status) == 0;
}

#else // _WIN32

static void
print_error (char const* name)
{
  LPTSTR msg;
  DWORD e (GetLastError());

  if (!FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        0,
        e,
        MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &msg,
        0,
        0))
  {
    cerr << name << ": error: unknown error code " << e << endl;
    return;
  }

  cerr << name << ": error: " << msg << endl;
  LocalFree (msg);
}

static process_info
start_process (char const* args[], char const* name, bool out)
{
  HANDLE out_h[2];
  HANDLE in_h[2];
  SECURITY_ATTRIBUTES sa;

  sa.nLength = sizeof (SECURITY_ATTRIBUTES);
  sa.bInheritHandle = true;
  sa.lpSecurityDescriptor = 0;

  if (!CreatePipe (&out_h[0], &out_h[1], &sa, 0) ||
      !SetHandleInformation (out_h[1], HANDLE_FLAG_INHERIT, 0))
  {
    print_error (name);
    throw process_failure ();
  }

  if (out)
  {
    if (!CreatePipe (&in_h[0], &in_h[1], &sa, 0) ||
        !SetHandleInformation (in_h[0], HANDLE_FLAG_INHERIT, 0))
    {
      print_error (name);
      throw process_failure ();
    }
  }

  // Create the process.
  //
  path file (args[0]);

  // Do PATH search.
  //
  if (file.directory ().empty ())
    file = path_search (file);

  if (file.empty ())
  {
    cerr << args[0] << ": error: file not found" << endl;
    throw process_failure ();
  }

  // Serialize the arguments to string.
  //
  string cmd_line;
  for (char const** p (args); *p != 0; ++p)
  {
    if (p != args)
      cmd_line += ' ';

    cmd_line += *p;
  }

  // Prepare other info.
  //
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  memset (&si, 0, sizeof (STARTUPINFO));
  memset (&pi, 0, sizeof (PROCESS_INFORMATION));

  si.cb = sizeof(STARTUPINFO);
  if (out)
  {
    si.hStdError = in_h[1];
    si.hStdOutput = in_h[1];
  }
  else
  {
    si.hStdError = GetStdHandle (STD_ERROR_HANDLE);
    si.hStdOutput = GetStdHandle (STD_OUTPUT_HANDLE);
  }
  si.hStdInput = out_h[0];
  si.dwFlags |= STARTF_USESTDHANDLES;

  if (!CreateProcess (
        file.string ().c_str (),
        const_cast<char*> (cmd_line.c_str ()),
        0,    // Process security attributes.
        0,    // Primary thread security attributes.
        true, // Inherit handles.
        0,    // Creation flags.
        0,    // Use our environment.
        0,    // Use our current directory.
        &si,
        &pi))
  {
    print_error (name);
    throw process_failure ();
  }

  CloseHandle (pi.hThread);

  process_info r;
  r.id = pi.hProcess;
  r.out_fd = _open_osfhandle ((intptr_t) (out_h[1]), 0);

  if (r.out_fd == -1)
  {
    cerr << name << ": error: unable to obtain C file handle" << endl;
    throw process_failure ();
  }

  if (out)
  {
    r.in_fd = _open_osfhandle ((intptr_t) (in_h[0]), 0);

    if (r.in_fd == -1)
    {
      cerr << name << ": error: unable to obtain C file handle" << endl;
      throw process_failure ();
    }
  }
  else
    r.in_fd = 0;

  return r;
}

static bool
wait_process (process_info pi, char const* name)
{
  DWORD status;

  if (WaitForSingleObject (pi.id, INFINITE) != WAIT_OBJECT_0 ||
      !GetExitCodeProcess (pi.id, &status))
  {
    print_error (name);
    throw process_failure ();
  }

  CloseHandle (pi.id);
  return status == 0;
}

#endif // _WIN32
