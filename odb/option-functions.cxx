// file      : odb/option-functions.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

#include <odb/option-functions.hxx>

using namespace std;

void
process_options (options& o)
{
  // Set the default schema format depending on the database.
  //
  if (o.generate_schema () && o.schema_format ().empty ())
  {
    set<schema_format> f;

    switch (o.database ())
    {
    case database::mysql:
    case database::oracle:
    case database::pgsql:
      {
        f.insert (schema_format::sql);
        break;
      }
    case database::sqlite:
      {
        f.insert (schema_format::embedded);
        break;
      }
    case database::tracer:
      {
        break;
      }
    }

    o.schema_format (f);
  }
}
