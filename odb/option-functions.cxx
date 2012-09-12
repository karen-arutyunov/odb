// file      : odb/option-functions.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <set>

#include <odb/option-functions.hxx>

using namespace std;

void
process_options (options& o)
{
  // If --generate-schema-only was specified, then set --generate-schema
  // as well.
  //
  if (o.generate_schema_only ())
    o.generate_schema (true);

  // Set the default schema format depending on the database.
  //
  if (o.generate_schema () && o.schema_format ().empty ())
  {
    set<schema_format> f;

    switch (o.database ())
    {
    case database::mssql:
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
    }

    o.schema_format (f);
  }
}
