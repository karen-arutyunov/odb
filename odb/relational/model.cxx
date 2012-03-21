// file      : odb/relational/model.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/gcc.hxx>

#include <cassert>
#include <limits>
#include <sstream>

#include <odb/relational/model.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

namespace relational
{
  namespace model
  {
    // object_columns
    //
    string object_columns::
    default_ (semantics::data_member& m)
    {
      default_value* dv (0);

      semantics::type& t (utype (m));

      if (m.count ("default"))
        dv = &m.get<default_value> ("default");
      else if (t.count ("default"))
        dv = &t.get<default_value> ("default");
      else
        return ""; // No default value for this column.

      switch (dv->kind)
      {
      case default_value::reset:
        {
          // No default value.
          return "";
        }
      case default_value::null:
        {
          return default_null (m);
          break;
        }
      case default_value::boolean:
        {
          return default_bool (m, dv->value == "true");
          break;
        }
      case default_value::number:
        {
          tree n (dv->node);

          switch (TREE_CODE (n))
          {
          case INTEGER_CST:
            {
              HOST_WIDE_INT hwl (TREE_INT_CST_LOW (n));
              HOST_WIDE_INT hwh (TREE_INT_CST_HIGH (n));

              unsigned long long l (hwl);
              unsigned long long h (hwh);
              unsigned short width (HOST_BITS_PER_WIDE_INT);

              unsigned long long v ((h << width) + l);

              return default_integer (m, v, dv->value == "-");
              break;
            }
          case REAL_CST:
            {
              double v;

              REAL_VALUE_TYPE d (TREE_REAL_CST (n));

              if (REAL_VALUE_ISINF (d))
                v = numeric_limits<double>::infinity ();
              else if (REAL_VALUE_ISNAN (d))
                v = numeric_limits<double>::quiet_NaN ();
              else
              {
                char tmp[256];
                real_to_decimal (tmp, &d, sizeof (tmp), 0, true);
                istringstream is (tmp);
                is >> v;
              }

              if (dv->value == "-")
                v = -v;

              return default_float (m, v);
              break;
            }
          default:
            assert (false);
          }
          break;
        }
      case default_value::string:
        {
          return default_string (m, dv->value);
          break;
        }
      case default_value::enumerator:
        {
          return default_enum (m, dv->node, dv->value);
          break;
        }
      }

      return "";
    }

    cutl::shared_ptr<sema_rel::model>
    generate ()
    {
      context ctx;
      cutl::shared_ptr<sema_rel::model> m (new (shared) sema_rel::model);

      traversal::unit unit;
      traversal::defines unit_defines;
      typedefs unit_typedefs (false);
      traversal::namespace_ ns;
      instance<class_> c (*m);

      unit >> unit_defines >> ns;
      unit_defines >> c;
      unit >> unit_typedefs >> c;

      traversal::defines ns_defines;
      typedefs ns_typedefs (false);

      ns >> ns_defines >> ns;
      ns_defines >> c;
      ns >> ns_typedefs >> c;

      try
      {
        unit.dispatch (ctx.unit);
      }
      catch (sema_rel::duplicate_name const& e)
      {
        semantics::node& o (*e.orig.get<semantics::node*> ("cxx-node"));
        semantics::node& d (*e.dup.get<semantics::node*> ("cxx-node"));

        cerr << d.file () << ":" << d.line () << ":" << d.column ()
             << ": error: " << e.dup.kind () << " name '" << e.name
             << "' conflicts with an already defined " << e.orig.kind ()
             << " name"
             << endl;

        cerr << o.file () << ":" << o.line () << ":" << o.column ()
             << ": info: conflicting " << e.orig.kind () << " is "
             << "defined here"
             << endl;

        cerr << d.file () << ":" << d.line () << ":" << d.column ()
             << ": error: use '#pragma db column' or '#pragma db table' "
             << "to change one of the names"
             << endl;

        throw operation_failed ();
      }

      return m;
    }
  }
}
