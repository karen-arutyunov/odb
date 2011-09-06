// file      : odb/pragma.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_PRAGMA_HXX
#define ODB_PRAGMA_HXX

#include <odb/gcc.hxx>

#include <map>
#include <set>
#include <vector>
#include <string>

struct pragma
{
  enum mode_type {override, accumulate};

  // Check that the pragma is applicable to the declaration. Return true
  // on success, complain and return false otherwise.
  //
  typedef bool (*check_func) (tree decl,
                              std::string const& decl_name,
                              std::string const& prag_name,
                              location_t);

  pragma (mode_type m,
          std::string const& pn,
          std::string const& cn,
          std::string const& v,
          tree n,
          location_t l,
          check_func c)
    : mode (m),
      pragma_name (pn),
      context_name (cn),
      value (v),
      node (n),
      loc (l),
      check (c)
  {
  }

  bool
  operator< (pragma const& y) const
  {
    if (mode == override)
      return pragma_name < y.pragma_name;
    else
      return pragma_name < y.pragma_name ||
        (pragma_name == y.pragma_name && loc < y.loc);
  }

  mode_type mode;
  std::string pragma_name;  // Actual pragma name for diagnostics.
  std::string context_name; // Context entry name.
  std::string value;
  tree node;
  location_t loc;
  check_func check;
};

typedef std::vector<pragma> pragma_list;

// A set of pragmas. Insertion of a pragma with the same name and override
// mode overrides the old value.
//
struct pragma_set: std::set<pragma>
{
  typedef std::set<pragma> base;

  void
  insert (pragma const& p)
  {
    std::pair<iterator, bool> r (base::insert (p));

    if (!r.second)
    {
      pragma& x (const_cast<pragma&> (*r.first));
      x = p;
    }
  }

  template <typename I>
  void
  insert (I begin, I end)
  {
    for (; begin != end; ++begin)
      insert (*begin);
  }
};


// Position pragmas inside a class or namespace. The key for the
// namespace case is the global_namespace node.
//
typedef std::map<tree, pragma_list> loc_pragmas;

// Pragmas associated with this declaration.
//
typedef std::map<tree, pragma_set> decl_pragmas;

extern loc_pragmas loc_pragmas_;
extern decl_pragmas decl_pragmas_;

extern "C" void
register_odb_pragmas (void*, void*);

#endif // ODB_PRAGMA_HXX
