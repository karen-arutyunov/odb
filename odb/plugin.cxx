// file      : odb/plugin.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/gcc.hxx> // Keep it first.

#include <memory>  // std::auto_ptr
#include <string>
#include <vector>
#include <iostream>

#include <odb/pragma.hxx>
#include <odb/parser.hxx>
#include <odb/options.hxx>
#include <odb/generator.hxx>
#include <odb/semantics/unit.hxx>

using namespace std;
using namespace semantics;

int plugin_is_GPL_compatible;
auto_ptr<options const> options_;

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
    parser p (*options_, loc_pragmas_, decl_pragmas_);
    path file (main_input_filename);
    auto_ptr<unit> u (p.parse (global_namespace, file));

    generator g;
    g.generate (*options_, *u, file);
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

extern "C" int
plugin_init (plugin_name_args* plugin_info, plugin_gcc_version* version)
{
  int r (0);

  try
  {
    // Parse options.
    //
    {
      vector<string> argv_str;
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
