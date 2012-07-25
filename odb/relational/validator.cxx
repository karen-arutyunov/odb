// file      : odb/relational/validator.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <map>
#include <iostream>

#include <odb/diagnostics.hxx>
#include <odb/traversal.hxx>
#include <odb/relational/common.hxx>
#include <odb/relational/context.hxx>
#include <odb/relational/validator.hxx>

using namespace std;

namespace relational
{
  namespace
  {
    //
    // Pass 2.
    //

    struct class2: traversal::class_, context
    {
      class2 (bool& valid)
          : valid_ (valid)
      {
      }

      virtual void
      traverse (type& c)
      {
        if (object (c))
          traverse_object (c);
        else if (view (c))
          traverse_view (c);
        else if (composite (c))
          traverse_composite (c);

        // Make sure indexes are not defined for anything other than objects.
        //
        if (c.count ("index") && !object (c))
        {
          indexes& ins (c.get<indexes> ("index"));

          for (indexes::iterator i (ins.begin ()); i != ins.end (); ++i)
          {
            error (i->loc) << "index definition on a non-persistent class"
                           << endl;
            valid_ = false;
          }
        }
      }

      virtual void
      traverse_object (type& c)
      {
        // Validate indexes.
        //
        {
          indexes& ins (c.get<indexes> ("index"));

          // Make sure index members are not transient or containers.
          //
          for (indexes::iterator i (ins.begin ()); i != ins.end (); ++i)
          {
            index& in (*i);

            for (index::members_type::iterator i (in.members.begin ());
                 i != in.members.end (); ++i)
            {
              index::member& im (*i);
              semantics::data_member& m (*im.path.back ());

              if (transient (m))
              {
                error (im.loc) << "index member is transient" << endl;
                valid_ = false;
              }

              if (container (m))
              {
                error (im.loc) << "index member is a container" << endl;
                valid_ = false;
              }
            }
          }
        }
      }

      virtual void
      traverse_view (type&)
      {
      }

      virtual void
      traverse_composite (type&)
      {
      }

    public:
      bool& valid_;
    };
  }

  void validator::
  validate (options const&,
            features&,
            semantics::unit& u,
            semantics::path const&,
            unsigned short pass)
  {
    bool valid (true);

    // Validate custom type mapping.
    //
    if (pass == 1)
    {
      // Create an empty list if we don't have one. This makes the
      // rest of the code simpler.
      //
      if (!u.count ("custom-db-types"))
        u.set ("custom-db-types", custom_db_types ());

      custom_db_types & cts (u.get<custom_db_types> ("custom-db-types"));

      for (custom_db_types::iterator i (cts.begin ()); i != cts.end (); ++i)
      {
        custom_db_type& ct (*i);

        if (ct.type.empty ())
        {
          error (ct.loc) << "'type' clause expected in db pragma map" << endl;
          valid = false;
        }

        if (ct.as.empty ())
        {
          error (ct.loc) << "'as' clause expected in db pragma map" << endl;
          valid = false;
        }

        if (ct.to.empty ())
          ct.to = "(?)";
        else
        {
          size_t p (ct.to.find ("(?)"));

          if (p == string::npos)
          {
            error (ct.loc) << "no '(?)' expression in the 'to' clause "
                           << "of db pragma map" << endl;
            valid = false;
          }
          else if (ct.to.find ("(?)", p + 3) != string::npos)
          {
            error (ct.loc) << "multiple '(?)' expressions in the 'to' "
                           << "clause of db pragma map" << endl;
            valid = false;
          }
        }

        if (ct.from.empty ())
          ct.from = "(?)";
        else
        {
          size_t p (ct.from.find ("(?)"));

          if (p == string::npos)
          {
            error (ct.loc) << "no '(?)' expression in the 'from' clause "
                           << "of db pragma map" << endl;
            valid = false;
          }
          else if (ct.from.find ("(?)", p + 3) != string::npos)
          {
            error (ct.loc) << "multiple '(?)' expressions in the 'from' "
                           << "clause of db pragma map" << endl;
            valid = false;
          }
        }
      }
    }

    if (!valid)
      throw failed ();

    if (pass == 1)
    {
    }
    else
    {
      traversal::unit unit;
      traversal::defines unit_defines;
      typedefs unit_typedefs (true);
      traversal::namespace_ ns;
      class2 c (valid);

      unit >> unit_defines >> ns;
      unit_defines >> c;
      unit >> unit_typedefs >> c;

      traversal::defines ns_defines;
      typedefs ns_typedefs (true);

      ns >> ns_defines >> ns;
      ns_defines >> c;
      ns >> ns_typedefs >> c;

      unit.dispatch (u);
    }

    if (!valid)
      throw failed ();
  }
}
