// file      : odb/profile.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <unistd.h>    // stat
#include <sys/types.h> // stat
#include <sys/stat.h>  // stat

#include <iostream>

#include <odb/profile.hxx>

using namespace std;

string
profile_search (char const* prof, void* arg)
{
  typedef profile_data::path path;
  typedef profile_data::paths paths;

  profile_data* pd (static_cast<profile_data*> (arg));
  paths const& ps (pd->search_paths);

  path p (prof), odb ("odb"), r;
  p.normalize (); // Convert '/' to the canonical path separator form.
  p += ".options";

  struct stat info;
  paths::const_iterator i (ps.begin ()), end (ps.end ());

  for (; i != end; ++i)
  {
    // First check in the search directory itself and then try the odb/
    // subdirectory.
    //
    r = *i / p;

    // Just check that the file exist without checking for permissions, etc.
    //
    if (stat (r.string ().c_str (), &info) == 0 && S_ISREG (info.st_mode))
      break;

    r = *i / odb / p;

    if (stat (r.string ().c_str (), &info) == 0 && S_ISREG (info.st_mode))
      break;
  }

  if (i == end)
  {
    cerr << pd->name << ": error: unable to locate options file for profile '"
         << prof << "'" << endl;
    throw profile_failure ();
  }

  if (pd->loaded.find (r) != pd->loaded.end ())
    return string ();

  pd->loaded.insert (r);
  return r.string ();
}
