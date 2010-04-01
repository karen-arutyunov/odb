// file      : odb/source.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/source.hxx>

namespace
{
  struct class_: traversal::class_, context
  {
    class_ (context& c)
        : context (c)
    {
    }

    virtual void
    traverse (type& c)
    {
      if (c.file () != unit.file () || !odb_class (c))
        return;

      string const& name (c.name ());

      os << "// " << c.name () << endl
         << "//" << endl;

      os << name << "::" << endl
         << name << " (::odb::image& i)"
         << "{"
         << "}";
    }

  private:
    bool
    odb_class (type& c)
    {
      // See if this class defines the ODB-specific c-tor.
      //
      tree t (c.tree_node ());

      for (tree f (TYPE_METHODS (t)); f != 0; f = TREE_CHAIN (f))
      {
        if (DECL_CONSTRUCTOR_P (f))
        {
          // Get the argument list and skip the first (this) argument.
          //
          tree a (TREE_CHAIN (DECL_ARGUMENTS (f)));

          if (a == 0)
            continue;

          tree at (TREE_TYPE (a));

          // Check that it is ::odb::image&.
          //
          if (TREE_CODE (at) != REFERENCE_TYPE)
            continue;

          tree rt (TREE_TYPE (at));
          tree mt (TYPE_MAIN_VARIANT (rt));

          semantics::node* node (unit.find (mt));

          if (node == 0)
            continue;

          semantics::type* t_node (dynamic_cast<semantics::type*> (node));

          if (t_node == 0)
            continue;

          if (t_node->anonymous () || t_node->fq_name () != "::odb::image")
            continue;

          // Make sure it is unqualified.
          //
          if (cp_type_quals (rt) != TYPE_UNQUALIFIED)
            continue; // @@ Should probably be an error/warning.

          // Check that it is the only argument.
          //
          if (TREE_CHAIN (a) != 0)
            continue; // @@ Should probably be an error/warning.

          return true;
        }
      }

      return false;
    }
  };
}

void
generate_source (context& ctx)
{
  traversal::unit unit;
  traversal::defines unit_defines;
  namespace_ ns (ctx);
  class_ c (ctx);

  unit >> unit_defines >> ns;
  unit_defines >> c;

  traversal::defines ns_defines;

  ns >> ns_defines >> ns;
  ns_defines >> c;

  unit.dispatch (ctx.unit);
}
