// file      : odb/profile.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_PROFILE_HXX
#define ODB_PROFILE_HXX

#include <set>
#include <vector>
#include <string>

#include <cutl/fs/path.hxx>

struct profile_data
{
  typedef cutl::fs::path path;
  typedef std::vector<path> paths;

  profile_data (paths const& p, char const* n): search_paths (p), name (n) {}

  paths const& search_paths;
  std::set<path> loaded;
  char const* name;
};

struct profile_failure {};

std::string
profile_search (char const* profile, void* arg);

#endif // ODB_PROFILE_HXX
