// file      : odb/parser.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_PARSER_HXX
#define ODB_PARSER_HXX

#include <odb/gcc.hxx>

#include <set>
#include <string>
#include <memory>  // std::auto_ptr
#include <ostream>

#include <odb/pragma.hxx>
#include <odb/options.hxx>
#include <odb/semantics.hxx>

class parser
{
public:
  class failed {};

  parser (options const& ops, loc_pragmas const&, decl_pragmas const&);

  std::auto_ptr<semantics::unit>
  parse (tree global_scope, semantics::path const& main_file);

private:
  typedef semantics::path path;
  typedef semantics::access access;

  // Extended GGC tree declaration that is either a tree node or a
  // pragma. If this declaration is a pragma, then the assoc flag
  // indicated whether this pragma has been associated with a
  // declaration.
  //
  struct tree_decl
  {
    tree decl;
    pragma const* prag;
    mutable bool assoc; // Allow modification via std::set iterator.

    tree_decl (tree d): decl (d), prag (0) {}
    tree_decl (pragma const& p): decl (0), prag (&p), assoc (false) {}

    bool
    operator< (tree_decl const& y) const;
  };

  typedef std::multiset<tree_decl> decl_set;

private:
  void
  collect (tree ns);

  void
  emit ();

  // Emit a type declaration. This is either a named class-type definition/
  // declaration or a typedef. In the former case the function returns the
  // newly created type node. In the latter case it returns 0.
  //
  semantics::type*
  emit_type_decl (tree);

  // Emit a template declaration.
  //
  void
  emit_template_decl (tree);

  semantics::class_template&
  emit_class_template (tree, bool stub = false);

  semantics::union_template&
  emit_union_template (tree, bool stub = false);

  template <typename T>
  T&
  emit_class (tree, path const& f, size_t l, size_t c, bool stub = false);

  template <typename T>
  T&
  emit_union (tree, path const& f, size_t l, size_t c, bool stub = false);

  semantics::enum_&
  emit_enum (tree, path const& f, size_t l, size_t c, bool stub = false);

  // Create new or find existing semantic graph type.
  //
  semantics::type&
  emit_type (tree, path const& f, size_t l, size_t c);

  semantics::type&
  create_type (tree, path const& f, size_t l, size_t c);

  std::string
  emit_type_name (tree, bool direct = true);


  // Pragma handling.
  //
  void
  process_pragmas (tree,
                   semantics::node&,
                   std::string const& name,
                   decl_set::const_iterator begin,
                   decl_set::const_iterator cur,
                   decl_set::const_iterator end);

  void
  diagnose_unassoc_pragmas (decl_set const&);

  // Return declaration's fully-qualified scope name (e.g., ::foo::bar).
  //
  std::string
  fq_scope (tree);

  // Return declaration's access.
  //
  access
  decl_access (tree);

  //
  //
  template <typename T>
  void
  define_fund (tree);

private:
  options const& ops_;
  loc_pragmas const& loc_pragmas_;
  decl_pragmas const& decl_pragmas_;

  bool trace;
  std::ostream& ts;

  semantics::unit* unit_;
  semantics::scope* scope_;

  std::size_t error_;

  decl_set decls_;
};


#endif // ODB_PARSER_HXX
