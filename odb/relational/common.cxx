// file      : odb/relational/common.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <string>
#include <cstddef>  // std::size_
#include <cstdlib>  // abort
#include <sstream>
#include <cxxabi.h> // abi::__cxa_demangle

#include <odb/relational/common.hxx>

using namespace std;

namespace relational
{
  // query_columns_base
  //

  query_columns_base::
  query_columns_base ()
      : decl_ (true)
  {
  }

  query_columns_base::
  query_columns_base (semantics::class_& c) //@@ context::{cur,top}_object
      : decl_ (false)
  {
    scope_ = "query_columns_base< " + c.fq_name () + " >";
  }

  void query_columns_base::
  traverse_object (semantics::class_& c)
  {
    // We don't want to traverse bases.
    //
    names (c);
  }

  void query_columns_base::
  traverse_composite (semantics::data_member* m, semantics::class_& c)
  {
    // Base type.
    //
    if (m == 0)
    {
      object_columns_base::traverse_composite (m, c);
      return;
    }

    // Don't generate an empty struct if we don't have any pointers.
    //
    if (!has_a (c, test_pointer))
      return;

    string name (public_name (*m));

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "struct " << name << "_base_"
         << "{";

      object_columns_base::traverse_composite (m, c);

      os << "};";
    }
    else
    {
      string old_scope (scope_);
      scope_ += "::" + name + "_base_";

      object_columns_base::traverse_composite (m, c);

      scope_ = old_scope;
    }
  }

  bool query_columns_base::
  traverse_column (semantics::data_member& m, string const& column, bool)
  {
    semantics::class_* c (object_pointer (m.type ()));

    if (c == 0)
      return false;

    string name (public_name (m));

    if (decl_)
      os << "// " << name << endl
         << "//" << endl
         << "static const char " << name << "_alias_[];"
         << endl
         << "typedef" << endl
         << "odb::pointer_query_columns< " << c->fq_name () << ", " <<
        name << "_alias_ >" << endl
         << name << ";"
         << endl;
    else
      // For now use column name. This will become problematic when we
      // add support for composite ids.
      //
      os << "const char " << scope_ <<  "::" << name << "_alias_[] =" << endl
         << strlit (column) << ";"
         << endl;

    return true;
  }

  // query_columns
  //

  query_columns::
  query_columns (bool ptr)
      : ptr_ (ptr), decl_ (true)
  {
  }

  query_columns::
  query_columns (bool ptr, semantics::class_& c) //@@ context::{cur,top}_object
      : ptr_ (ptr), decl_ (false)
  {
    scope_ = ptr ? "pointer_query_columns" : "query_columns";
    scope_ += "< " + c.fq_name () + ", table >";
  }

  void query_columns::
  traverse_object (semantics::class_& c)
  {
    // We don't want to traverse bases.
    //
    names (c);
  }

  void query_columns::
  traverse_composite (semantics::data_member* m, semantics::class_& c)
  {
    // Base type.
    //
    if (m == 0)
    {
      object_columns_base::traverse_composite (m, c);
      return;
    }

    string name (public_name (*m));

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "struct " << name;

      // Derive from the base in query_columns_base. It contains columns
      // for the pointer members.
      //
      if (!ptr_ && has_a (c, test_pointer))
        os << ": " << name << "_base_";

      os << "{";

      object_columns_base::traverse_composite (m, c);

      os << "};";
    }
    else
    {
      string old_scope (scope_);
      scope_ += "::" + name;

      object_columns_base::traverse_composite (m, c);

      scope_ = old_scope;
    }
  }

  bool query_columns::
  traverse_column (semantics::data_member& m, string const& column, bool)
  {
    string mtype;

    if (semantics::class_* c = object_pointer (m.type ()))
    {
      // If this is for the pointer_query_columns and the member is not
      // inverse, then create the normal member corresponding to the id
      // column. This will allow the user to check it for NULL or to
      // compare ids. In case this is for query_columns, then the
      // corresponding member is defined in query_columns_base.
      //
      if (!ptr_ || inverse (m))
        return false;

      semantics::data_member& id (*id_member (*c));
      mtype = id.type ().fq_name (id.belongs ().hint ());
    }
    else
      mtype = m.type ().fq_name (m.belongs ().hint ());

    string name (public_name (m));
    string db_type_id (database_type_id (m));

    string type (
      string (db.string ()) + "::value_traits< "
      + mtype + ", "
      + db_type_id
      + " >::query_type");

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "static const " << db << "::query_column<" << endl
         << "  " << type << "," << endl
         << "  " << db_type_id << " >" << endl
         << name << ";"
         << endl;
    }
    else
    {
      os << "template <const char* table>" << endl
         << "const " << db << "::query_column<" << endl
         << "  " << type << "," << endl
         << "  " << db_type_id << " >" << endl
         << scope_ << "::" << name << " (" << endl
         << "table, " << strlit (column) << ");"
         << endl;
    }

    return true;
  }

  //
  // Dynamic traversal support.
  //

  struct demangled_name
  {
    demangled_name (): s (0), n (0) {}
    ~demangled_name () {free (s);}
    char* s;
    size_t n;
  };

  static demangled_name name_;

  database entry_base::
  db (type_info const& ti)
  {
    char*& s (name_.s);

    int r;
    s = abi::__cxa_demangle (ti.name (), s, &name_.n, &r);

    if (r != 0)
      abort (); // We are in static initialization, so this is fatal.

    string str (s + 12); // 12 for "relational::"
    istringstream is (string (str, 0, str.find (':')));

    database d;
    if (!(is >> d))
      abort ();

    return d;
  }
}
