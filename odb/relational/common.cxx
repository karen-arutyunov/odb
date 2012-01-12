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
    scope_ = "query_columns_base< " + class_fq_name (c) + " >";
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
    semantics::class_* ptr (object_pointer (utype (m)));

    if (ptr == 0)
      return false;

    string name (public_name (m));
    bool inv (inverse (m));

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "static const char " << name << "_alias_[];"
         << endl;

      if (inv)
      {
        os << "typedef" << endl
           << "odb::query_pointer<" << endl
           << "  odb::pointer_query_columns<" << endl
           << "    " << class_fq_name (*ptr) << "," << endl
           << "    " << name << "_alias_ > >" << endl
           << name << "_type_ ;"
           << endl
           << "static const " << name << "_type_ " << name << ";"
           << endl;
      }
    }
    else
    {
      // For now use column name as table alias. This will become problematic
      // when we add support for composite ids.
      //
      os << "const char " << scope_ <<  "::" << name << "_alias_[] = " <<
         strlit (column) << ";"
         << endl;

      if (inv)
        os << "const " << scope_ << "::" << name << "_type_" << endl
           << scope_ << "::" << name << ";"
           << endl;
    }

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
    scope_ += "< " + class_fq_name (c) + ", table >";
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
         << "struct " << name << "_type_";

      // Derive from the base in query_columns_base. It contains columns
      // data for the pointer members.
      //
      if (!ptr_ && has_a (c, test_pointer))
        os << ": " << name << "_base_";

      os << "{"
         << name << "_type_ (){}"; // For some reason GCC needs this c-tor
                                   // if we make the static member const.

      object_columns_base::traverse_composite (m, c);

      os << "};"
         << "static const " << name << "_type_ " << name << ";"
         << endl;
    }
    else
    {
      // Handle nested members first.
      //
      string old_scope (scope_);
      scope_ += "::" + name + "_type_";

      object_columns_base::traverse_composite (m, c);

      scope_ = old_scope;

      // Composite member.
      //
      os << "template <const char* table>" << endl
         << "const typename " << scope_ << "::" << name << "_type_" << endl
         << scope_ << "::" << name << ";"
         << endl;
    }
  }

  bool query_columns::
  traverse_column (semantics::data_member& m, string const& column, bool)
  {
    semantics::names* hint;
    semantics::type& t (utype (m, hint));
    semantics::class_* ptr (object_pointer (t));

    if (ptr != 0)
    {
      // If this is for the pointer_query_columns and the member is not
      // inverse, then create the normal member corresponding to the id
      // column. This will allow the user to check it for NULL or to
      // compare ids. In case this is for query_columns, then for the
      // inverse member everything has been generated in query_columns_base.
      //
      if (inverse (m))
        return false;
    }

    string name (public_name (m));

    if (decl_)
    {
      string type;
      if (ptr != 0)
      {
        semantics::data_member& id (*id_member (*ptr));
        semantics::type& t (utype (id, hint));
        type = t.fq_name (hint);
      }
      else
        type = t.fq_name (hint);

      string type_id (database_type_id (m));

      os << "// " << name << endl
         << "//" << endl;

      os << "typedef" << endl
         << db << "::query_column<" << endl
         << "  " << db << "::value_traits<" << endl
         << "    " << type << "," << endl
         << "    " << type_id << " >::query_type," << endl
         << "  " << type_id << " >" << endl;

      if (ptr == 0 || ptr_)
        os << name << "_type_;"
           << endl;
      else
      {
        os << name << "_column_type_;"
           << endl
           << "typedef" << endl
           << "odb::query_pointer<" << endl
           << "  odb::pointer_query_columns<" << endl
           << "    " << class_fq_name (*ptr) << "," << endl
           << "    " << name << "_alias_ > >" << endl
           << name << "_pointer_type_;"
           << endl;

        // If this is a non-inverse relationship, then make the column have
        // a dual interface: that of an object pointer and of an id column.
        // The latter allows the user to, for example, use the is_null()
        // test in a natural way. For inverse relationships there is no
        // column and so the column interface is not available.
        //
        os << "struct " << name << "_type_: " <<
          name << "_pointer_type_, " << name << "_column_type_"
           << "{"
           << name << "_type_ (const char* t, const char* c)" << endl
           << "  : " << name << "_column_type_ (t, c)"
           << "{"
           << "}"
           << "};";
      }

      os << "static const " << name << "_type_ " << name << ";"
         << endl;
    }
    else
    {
      os << "template <const char* table>" << endl
         << "const typename " << scope_ << "::" << name << "_type_" << endl
         << scope_ << "::" << name << " (" << "table, " << strlit (column);

      column_ctor_extra (m);

      os << ");"
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
