// file      : odb/relational/schema-source.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema-source.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

namespace relational
{
  namespace schema
  {
    void
    generate_source ()
    {
      context ctx;
      ostream& os (ctx.os);
      database db (ctx.db);
      options const& ops (ctx.options);
      sema_rel::model& model (*ctx.model);
      string const& schema_name (ops.schema_name ()[db]);

      instance<cxx_emitter> emitter;
      emitter_ostream emitter_os (*emitter);
      schema_format format (schema_format::embedded);

      if (!model.names_empty ())
      {
        os << "namespace odb"
           << "{"
           << "static bool" << endl
           << "create_schema (database& db, unsigned short pass, bool drop)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << "ODB_POTENTIALLY_UNUSED (pass);"
           << "ODB_POTENTIALLY_UNUSED (drop);"
           << endl;

        // Drop.
        //
        if (!ops.omit_drop ())
        {
          bool close (false);

          os << "if (drop)"
             << "{";

          instance<drop_model> dmodel (*emitter, emitter_os, format);
          instance<drop_table> dtable (*emitter, emitter_os, format);
          trav_rel::qnames names;
          dmodel >> names >> dtable;

          for (unsigned short pass (1); pass < 3; ++pass)
          {
            emitter->pass (pass);
            dmodel->pass (pass);
            dtable->pass (pass);

            dmodel->traverse (model);

            close = close || !emitter->empty ();
          }

          if (close) // Close the last case and the switch block.
            os << "return false;"
               << "}"  // case
               << "}";  // switch

          os << "}";
        }

        // Create.
        //
        if (!ops.omit_create ())
        {
          bool close (false);

          if (ops.omit_drop ())
            os << "if (!drop)";
          else
            os << "else";

          os << "{";

          instance<create_model> cmodel (*emitter, emitter_os, format);
          instance<create_table> ctable (*emitter, emitter_os, format);
          trav_rel::qnames names;
          cmodel >> names >> ctable;

          for (unsigned short pass (1); pass < 3; ++pass)
          {
            emitter->pass (pass);
            cmodel->pass (pass);
            ctable->pass (pass);

            cmodel->traverse (model);

            close = close || !emitter->empty ();
          }

          if (close) // Close the last case and the switch block.
            os << "return false;"
               << "}"  // case
               << "}"; // switch

          os << "}";
        }

        os << "return false;"
           << "}";

        os << "static const schema_catalog_entry" << endl
           << "create_schema_entry_ (" << endl
           << "id_" << db << "," << endl
           << context::strlit (schema_name) << "," << endl
           << "&create_schema);"
           << endl;

        os << "}";
      }
    }
  }
}
