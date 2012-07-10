// file      : odb/relational/validator.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <iostream>

#include <odb/diagnostics.hxx>
#include <odb/traversal.hxx>
#include <odb/relational/common.hxx>
#include <odb/relational/context.hxx>
#include <odb/relational/validator.hxx>

using namespace std;

namespace relational
{
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
  }
}
