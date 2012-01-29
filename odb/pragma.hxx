// file      : odb/pragma.hxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_PRAGMA_HXX
#define ODB_PRAGMA_HXX

#include <odb/gcc.hxx>

#include <map>
#include <set>
#include <vector>
#include <string>

#include <cutl/container/any.hxx>
#include <cutl/compiler/context.hxx>

struct pragma
{
  // Check that the pragma is applicable to the declaration. Return true
  // on success, complain and return false otherwise.
  //
  typedef bool (*check_func) (tree decl,
                              std::string const& decl_name,
                              std::string const& prag_name,
                              location_t);

  // Add the pragma value to the context.
  //
  typedef void (*add_func) (cutl::compiler::context&,
                            std::string const& key,
                            cutl::container::any const& value,
                            location_t);

  pragma (std::string const& pn,
          std::string const& cn,
          cutl::container::any const& v,
          location_t l,
          check_func c,
          add_func a)
    : pragma_name (pn),
      context_name (cn),
      value (v),
      loc (l),
      check (c),
      add (a)
  {
  }

  bool
  operator< (pragma const& y) const
  {
    if (add == 0)
      return context_name < y.context_name;
    else
      return context_name < y.context_name ||
        (context_name == y.context_name && loc < y.loc);
  }

  std::string pragma_name;  // Actual pragma name for diagnostics.
  std::string context_name; // Context entry name.
  cutl::container::any value;
  location_t loc;
  check_func check;
  add_func add;
};

typedef std::vector<pragma> pragma_list;

// A set of pragmas. Insertion of a pragma with the same name and no
// custom add function overrides the old value.
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
extern loc_pragmas loc_pragmas_;

// Position pragmas for namespaces. Because re-opened namespaces do
// not have any representation in the GCC tree, these are handled in
// a special way. They are stored as a list of pragmas and their outer
// namespaces.
//
struct ns_loc_pragma
{
  typedef ::pragma pragma_type;
  ns_loc_pragma (tree n, pragma_type const& p): ns (n), pragma (p) {}

  tree ns;
  pragma_type pragma;
};

typedef std::vector<ns_loc_pragma> ns_loc_pragmas;
extern ns_loc_pragmas ns_loc_pragmas_;

// Pragmas associated with this declaration.
//
typedef std::map<tree, pragma_set> decl_pragmas;
extern decl_pragmas decl_pragmas_;

// List of pragma names (in context name form) that disqualify a value
// type from being treated as composite (i.e., simple value pragmas).
//
typedef std::set<std::string> pragma_name_set;
extern pragma_name_set simple_value_pragmas_;

extern "C" void
register_odb_pragmas (void*, void*);

struct pragmas_failed {};

void
post_process_pragmas ();

#endif // ODB_PRAGMA_HXX
