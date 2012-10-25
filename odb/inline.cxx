// file      : odb/inline.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/generate.hxx>
#include <odb/diagnostics.hxx>

using namespace std;

namespace inline_
{
  //
  //
  struct callback_calls: traversal::class_, virtual context
  {
    callback_calls ()
    {
      *this >> inherits_ >> *this;
    }

    void
    traverse (type& c, bool constant)
    {
      const_ = constant;
      traverse (c);
    }

    virtual void
    traverse (type& c)
    {
      bool obj (object (c));

      // Ignore transient bases.
      //
      if (!(obj || view (c)))
        return;

      if (c.count ("callback"))
      {
        string name (c.get<string> ("callback"));

        // In case of the const instance, we only generate the call if
        // there is a const callback. Note also that we cannot use
        // object_type/view_type alias because it can be a base type.
        //
        string const& type (class_fq_name (c));

        if (const_)
        {
          if (c.count ("callback-const"))
            os << "static_cast<const " << type << "&> (x)." << name <<
              " (e, db);";
        }
        else
          os << "static_cast< " << type << "&> (x)." << name << " (e, db);";
      }
      else if (obj)
        inherits (c);
    }

  protected:
    bool const_;
    traversal::inherits inherits_;
  };

  struct class_: traversal::class_, virtual context
  {
    virtual void
    traverse (type& c)
    {
      if (!options.at_once () && class_file (c) != unit.file ())
        return;

      if (object (c))
        traverse_object (c);
      else if (view (c))
        traverse_view (c);
    }

    void
    traverse_object (type&);

    void
    traverse_view (type&);

  private:
    callback_calls callback_calls_;
  };
}

void inline_::class_::
traverse_object (type& c)
{
  using semantics::data_member;

  data_member* id (id_member (c));
  bool base_id (id && &id->scope () != &c); // Comes from base.

  // Base class that contains the object id.
  //
  type* base (id != 0 && base_id ? dynamic_cast<type*> (&id->scope ()) : 0);

  bool poly (polymorphic (c));
  bool abst (abstract (c));
  bool reuse_abst (abst && !poly);

  string const& type (class_fq_name (c));
  string traits ("access::object_traits< " + type + " >");

  os << "// " << class_name (c) << endl
     << "//" << endl
     << endl;

  // id (object_type)
  //
  if (id != 0 || !reuse_abst)
  {
    os << "inline" << endl
       << traits << "::id_type" << endl
       << traits << "::" << endl
       << "id (const object_type&" << (id != 0 ? " o" : "") << ")"
       << "{";

    if (id != 0)
    {
      if (base_id)
        os << "return object_traits< " << class_fq_name (*base) <<
          " >::id (o);";
      else
      {
        // Get the id using the accessor expression. If this is not
        // a synthesized expression, then output its location for
        // easier error tracking.
        //
        member_access& ma (id->get<member_access> ("get"));

        if (!ma.synthesized)
          os << "// From " << location_string (ma.loc, true) << endl;

        os << "return " << ma.translate ("o") << ";";
      }
    }

    os << "}";
  }

  // The rest does not apply to reuse-abstract objects.
  //
  if (reuse_abst)
    return;

  // callback ()
  //
  os << "inline" << endl
     << "void " << traits << "::" << endl
     << "callback (database& db, object_type& x, callback_event e)"
     <<  endl
     << "{"
     << "ODB_POTENTIALLY_UNUSED (db);"
     << "ODB_POTENTIALLY_UNUSED (x);"
     << "ODB_POTENTIALLY_UNUSED (e);"
     << endl;
  callback_calls_.traverse (c, false);
  os << "}";

  os << "inline" << endl
     << "void " << traits << "::" << endl
     << "callback (database& db, const object_type& x, callback_event e)"
     << "{"
     << "ODB_POTENTIALLY_UNUSED (db);"
     << "ODB_POTENTIALLY_UNUSED (x);"
     << "ODB_POTENTIALLY_UNUSED (e);"
     << endl;
  callback_calls_.traverse (c, true);
  os << "}";
}

void inline_::class_::
traverse_view (type& c)
{
  string const& type (class_fq_name (c));
  string traits ("access::view_traits< " + type + " >");

  os << "// " << class_name (c) << endl
     << "//" << endl
     << endl;

  // callback ()
  //
  os << "inline" << endl
     << "void " << traits << "::" << endl
     << "callback (database& db, view_type& x, callback_event e)"
     <<  endl
     << "{"
     << "ODB_POTENTIALLY_UNUSED (db);"
     << "ODB_POTENTIALLY_UNUSED (x);"
     << "ODB_POTENTIALLY_UNUSED (e);"
     << endl;
  callback_calls_.traverse (c, false);
  os << "}";
}

namespace inline_
{
  void
  generate ()
  {
    context ctx;
    ostream& os (ctx.os);

    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (false);
    traversal::namespace_ ns;
    class_ c;

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (false);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

    os << "namespace odb"
       << "{";

    unit.dispatch (ctx.unit);

    os << "}";
  }
}
