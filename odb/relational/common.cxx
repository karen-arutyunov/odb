// file      : odb/relational/common.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
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

  void query_columns_base::
  traverse_pointer (semantics::data_member& m, semantics::class_& c)
  {
    string name (public_name (m));
    bool inv (inverse (m, key_prefix_));

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
           << "    " << class_fq_name (c) << "," << endl
           << "    " << name << "_alias_ > >" << endl
           << name << "_type_ ;"
           << endl
           << "static const " << name << "_type_ " << name << ";"
           << endl;
      }
    }
    else
    {
      // Come up with a table alias. Generally, we want it to be based
      // on the column name. This is straightforward for single-column
      // references. In case of a composite id, we will need to use the
      // column prefix which is based on the data member name, unless
      // overridden by the user. In the latter case the prefix can be
      // empty, in which case we will just fall back on the member's
      // public name.
      //
      string alias;

      if (composite_wrapper (utype ((*id_member (c)))))
      {
        string p (column_prefix (m, key_prefix_, default_name_));

        if (p.empty ())
          p = public_name_db (m);
        else
          p.resize (p.size () - 1); // Remove trailing underscore.

        alias = column_prefix_ + p;
      }
      else
        alias = column_prefix_ + column_name (m, key_prefix_, default_name_);

      os << "const char " << scope_ <<  "::" << name << "_alias_[] = " <<
        strlit (quote_id (alias)) << ";"
         << endl;

      if (inv)
        os << "const " << scope_ << "::" << name << "_type_" << endl
           << scope_ << "::" << name << ";"
           << endl;
    }
  }

  // query_columns
  //

  query_columns::
  query_columns (bool ptr)
      : ptr_ (ptr), decl_ (true), in_ptr_ (false)
  {
  }

  query_columns::
  query_columns (bool ptr, semantics::class_& c) //@@ context::{cur,top}_object
      : ptr_ (ptr), decl_ (false), in_ptr_ (false)
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
    string suffix (in_ptr_ ? "_column_type_" : "_type_");

    if (decl_)
    {
      os << "// " << name << endl
         << "//" << endl
         << "struct " << name << suffix;

      // Derive from the base in query_columns_base. It contains columns
      // data for the pointer members.
      //
      if (!ptr_ && has_a (c, test_pointer))
        os << ": " << name << "_base_";

      os << "{"
         << name << suffix << " (){}"; // For some reason GCC needs this c-tor
                                       // if we make the static member const.

      object_columns_base::traverse_composite (m, c);

      os << "};";

      if (!in_ptr_)
        os << "static const " << name << "_type_ " << name << ";"
           << endl;
    }
    else
    {
      // Handle nested members first.
      //
      string old_scope (scope_);
      scope_ += "::" + name + suffix;

      object_columns_base::traverse_composite (m, c);

      scope_ = old_scope;

      // Composite member. Note that here we don't use suffix.
      //
      os << "template <const char* table>" << endl
         << "const typename " << scope_ << "::" << name << "_type_" << endl
         << scope_ << "::" << name << ";"
         << endl;
    }
  }

  void query_columns::
  column_ctor (string const& type, string const& base)
  {
    os << type << " (const char* t, const char* c)" << endl
       << "  : " << base << " (t, c)"
       << "{"
       << "}";
  }

  void query_columns::
  column_common (semantics::data_member& m,
                 string const& type,
                 string const& column,
                 string const& suffix)
  {
    string name (public_name (m));

    if (decl_)
    {
      string type_id (database_type_id (m));

      os << "// " << name << endl
         << "//" << endl;

      os << "typedef" << endl
         << db << "::query_column<" << endl
         << "  " << db << "::value_traits<" << endl
         << "    " << type << "," << endl
         << "    " << type_id << " >::query_type," << endl
         << "  " << type_id << " >" << endl
         << name << suffix << ";"
         << endl;
    }
    else
    {
      // Note that here we don't use suffix.
      //
      os << "template <const char* table>" << endl
         << "const typename " << scope_ << "::" << name << "_type_" << endl
         << scope_ << "::" << name << " (" << "table, " <<
        strlit (quote_id (column));

      column_ctor_extra (m);

      os << ");"
         << endl;
    }
  }

  bool query_columns::
  traverse_column (semantics::data_member& m, string const& column, bool)
  {
    semantics::names* hint;
    semantics::type& t (utype (m, hint));

    column_common (m, t.fq_name (hint), column);

    if (decl_)
    {
      string name (public_name (m));

      os << "static const " << name << "_type_ " << name << ";"
         << endl;
    }

    return true;
  }

  void query_columns::
  traverse_pointer (semantics::data_member& m, semantics::class_& c)
  {
    // If this is for the pointer_query_columns and the member is not
    // inverse, then create the normal member corresponding to the id
    // column. This will allow the user to check it for NULL or to
    // compare ids. In case this is for query_columns, then for the
    // inverse member everything has been generated in query_columns_base.
    //
    if (inverse (m, key_prefix_))
      return;

    string name (public_name (m));

    semantics::data_member& id (*id_member (c));
    semantics::names* hint;
    semantics::type& t (utype (id, hint));

    if (composite_wrapper (t))
    {
      // Composite id.
      //

      // For pointer_query_columns generate normal composite mapping.
      //
      if (ptr_)
        object_columns_base::traverse_pointer (m, c);
      else
      {
        // If this is a non-inverse relationship, then make the column have
        // a dual interface: that of an object pointer and of an id column.
        // The latter allows the user to, for example, use the is_null()
        // test in a natural way. For inverse relationships there is no
        // column and so the column interface is not available.
        //
        in_ptr_ = true;
        object_columns_base::traverse_pointer (m, c);
        in_ptr_ = false;

        if (decl_)
        {
          os << "typedef" << endl
             << "odb::query_pointer<" << endl
             << "  odb::pointer_query_columns<" << endl
             << "    " << class_fq_name (c) << "," << endl
             << "    " << name << "_alias_ > >" << endl
             << name << "_pointer_type_;"
             << endl;

          os << "struct " << name << "_type_: " <<
            name << "_pointer_type_, " << name << "_column_type_"
             << "{"
             << "};";

          os << "static const " << name << "_type_ " << name << ";"
             << endl;
        }
      }
    }
    else
    {
      // Simple id.
      //
      string type (t.fq_name (hint));
      string column (column_prefix_ +
                     column_name (m, key_prefix_, default_name_));

      // For pointer_query_columns generate normal column mapping.
      //
      if (ptr_)
        column_common (m, type, column);
      else
      {
        // If this is a non-inverse relationship, then make the column have
        // a dual interface: that of an object pointer and of an id column.
        // The latter allows the user to, for example, use the is_null()
        // test in a natural way. For inverse relationships there is no
        // column and so the column interface is not available.
        //
        column_common (m, type, column, "_column_type_");

        if (decl_)
        {
          os << "typedef" << endl
             << "odb::query_pointer<" << endl
             << "  odb::pointer_query_columns<" << endl
             << "    " << class_fq_name (c) << "," << endl
             << "    " << name << "_alias_ > >" << endl
             << name << "_pointer_type_;"
             << endl;

          os << "struct " << name << "_type_: " <<
            name << "_pointer_type_, " << name << "_column_type_"
             << "{";

          column_ctor (name + "_type_", name + "_column_type_");

          os << "};";
        }
      }

      if (decl_)
        os << "static const " << name << "_type_ " << name << ";"
           << endl;
    }
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
