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
  // query_columns
  //

  query_columns::
  query_columns ()
      : ptr_ (true), decl_ (true)
  {
  }

  query_columns::
  query_columns (semantics::class_& cl) //@@ context::object
      : ptr_ (true), decl_ (false)
  {
    scope_ = "access::object_traits< " + cl.fq_name () + " >::query_type";
    table_ = table_qname (cl);
  }

  void query_columns::
  composite (semantics::data_member& m, semantics::class_& c)
  {
    string name (public_name (m));

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "struct " << name
         << "{";

      object_columns_base::composite (m, c);

      os << "};";
    }
    else
    {
      string old_scope (scope_);
      scope_ += "::" + name;

      object_columns_base::composite (m, c);

      scope_ = old_scope;
    }
  }

  bool query_columns::
  column (semantics::data_member& m, string const& col_name, bool)
  {
    string name (public_name (m));

    if (semantics::class_* c = object_pointer (m.type ()))
    {
      // We cannot just typedef the query_type from the referenced
      // object for two reasons: (1) it may not be defined yet and
      // (2) it will contain columns for its own pointers which
      // won't work (for now we only support one level of indirection
      // in queries). So we will have to duplicate the columns (sans
      // the pointers).
      //
      if (ptr_)
      {
        ptr_ = false;

        if (decl_)
        {
          os << "// " << name << endl
             << "//" << endl
             << "struct " << name
             << "{";

          traverse (*c);

          os << "};";
        }
        else
        {
          string old_scope (scope_), old_table (table_);
          scope_ += "::" + name;
          table_ = table_qname (*c);
          traverse (*c);
          table_ = old_table;
          scope_ = old_scope;
        }

        ptr_ = true;
      }
    }
    else
    {
      string db_type_id (database_type_id (m));

      string type (
        string (db.string ()) + "::value_traits< "
        + m.type ().fq_name (m.belongs ().hint ()) + ", "
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
        string column (table_ + '.' + quote_id (col_name));

        os << "const " << db << "::query_column<" << endl
           << "  " << type << "," << endl
           << "  " << db_type_id << " >" << endl
           << scope_ << "::" << name << " (" << endl
           << strlit (column) << ");"
           << endl;
      }
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
