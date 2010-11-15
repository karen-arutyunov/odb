// file      : odb/plugin.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx> // Keep it first.

#include <memory>  // std::auto_ptr
#include <string>
#include <vector>
#include <cstring> // std::strcpy
#include <iostream>

#include <odb/pragma.hxx>
#include <odb/parser.hxx>
#include <odb/options.hxx>
#include <odb/version.hxx>
#include <odb/validator.hxx>
#include <odb/generator.hxx>
#include <odb/semantics/unit.hxx>

using namespace std;
using namespace semantics;

int plugin_is_GPL_compatible;
auto_ptr<options const> options_;

// A prefix of the _cpp_file struct. This struct is not part of the
// public interface so we have to resort to this technique (based on
// libcpp/files.c).
//
struct cpp_file_prefix
{
  char const* name;
  char const* path;
  char const* pchname;
  char const* dir_name;
};

extern "C" void
start_unit_callback (void*, void*)
{
  // Set the directory of the main file (stdin) to that of the orginal
  // file.
  //
  cpp_buffer* b (cpp_get_buffer (parse_in));
  _cpp_file* f (cpp_get_file (b));
  char const* p (cpp_get_path (f));
  cpp_file_prefix* fp (reinterpret_cast<cpp_file_prefix*> (f));

  // Perform sanity checks.
  //
  if (p != 0 && *p == '\0'     // The path should be empty (stdin).
      && cpp_get_prev (b) == 0 // This is the only buffer (main file).
      && fp->path == p         // Our prefix corresponds to the actual type.
      && fp->dir_name == 0)    // The directory part hasn't been initialized.
  {
    path p (options_->svc_file ());
    path d (p.directory ());
    char* s;

    if (d.empty ())
    {
      s = XNEWVEC (char, 1);
      *s = '\0';
    }
    else
    {
      size_t n (d.string ().size ());
      s = XNEWVEC (char, n + 2);
      strcpy (s, d.string ().c_str ());
      s[n] = path::traits::directory_separator;
      s[n + 1] = '\0';
    }

    fp->dir_name = s;
  }
  else
  {
    cerr << "ice: unable to initialize main file directory" << endl;
    exit (1);
  }
}

extern "C" void
gate_callback (void*, void*)
{
  // If there were errors during compilation, let GCC handle the
  // exit.
  //
  if (errorcount || sorrycount)
    return;

  int r (0);

  try
  {
    path file (options_->svc_file ());
    parser p (*options_, loc_pragmas_, decl_pragmas_);
    auto_ptr<unit> u (p.parse (global_namespace, file));

    //
    //
    validator v;
    if (!v.validate (*options_, *u, file))
      r = 1;

    //
    //
    if (r == 0)
    {
      generator g;
      g.generate (*options_, *u, file);
    }
  }
  catch (parser::failed const&)
  {
    // Diagnostics has aready been issued.
    //
    r = 1;
  }
  catch (generator::failed const&)
  {
    // Diagnostics has aready been issued.
    //
    r = 1;
  }

  exit (r);
}

static char const* const odb_version = ODB_COMPILER_VERSION_STR;

typedef vector<string> strings;

extern "C" int
plugin_init (plugin_name_args* plugin_info, plugin_gcc_version*)
{
  int r (0);
  plugin_info->version = odb_version;

  try
  {
    // Parse options.
    //
    {
      strings argv_str;
      vector<char*> argv;

      argv_str.push_back (plugin_info->base_name);
      argv.push_back (const_cast<char*> (argv_str.back ().c_str ()));

      for (int i (0); i < plugin_info->argc; ++i)
      {
        plugin_argument& a (plugin_info->argv[i]);

        string opt (strlen (a.key) > 1 ? "--" : "-");
        opt += a.key;

        argv_str.push_back (opt);
        argv.push_back (const_cast<char*> (argv_str.back ().c_str ()));

        if (a.value != 0)
        {
          argv_str.push_back (a.value);
          argv.push_back (const_cast<char*> (argv_str.back ().c_str ()));
        }
      }

      int argc (static_cast<int> (argv.size ()));
      cli::argv_file_scanner scan (argc, &argv[0], "--options-file");

      options_.reset (
        new options (scan, cli::unknown_mode::fail, cli::unknown_mode::fail));
    }

    if (options_->trace ())
      cerr << "starting plugin " << plugin_info->base_name << endl;

    // Disable assembly output.
    //
    asm_file_name = HOST_BIT_BUCKET;

    // Register callbacks.
    //
    register_callback (plugin_info->base_name,
                       PLUGIN_PRAGMAS,
                       register_odb_pragmas,
                       0);

    register_callback (plugin_info->base_name,
                       PLUGIN_START_UNIT,
                       start_unit_callback,
                       0);

    register_callback (plugin_info->base_name,
                       PLUGIN_OVERRIDE_GATE,
                       &gate_callback,
                       0);
  }
  catch (cli::exception const& ex)
  {
    cerr << ex << endl;
    r = 1;
  }

  if (r != 0)
    exit (r);

  return r;
}
