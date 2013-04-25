// file      : odb/schema-catalog.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <map>
#include <vector>
#include <cassert>

#include <odb/database.hxx>
#include <odb/exceptions.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/schema-catalog-impl.hxx>

using namespace std;

namespace odb
{
  // Schema.
  //
  typedef bool (*create_function) (database&, unsigned short pass, bool drop);
  typedef bool (*migrate_function) (database&, unsigned short pass, bool pre);

  typedef pair<database_id, string> key;
  typedef vector<create_function> create_functions;
  typedef vector<migrate_function> migrate_functions;
  typedef map<schema_version, migrate_functions> version_map;

  struct schema_functions
  {
    create_functions create;
    version_map migrate;
  };
  typedef map<key, schema_functions> schema_map;

  // Data. Normally the code would be database-independent, though there
  // could be database-specific migration steps.
  //
  typedef pair<string, schema_version> data_key;

  struct data_function
  {
    typedef schema_catalog::data_migration_function_type function_type;

    data_function () {}
    data_function (database_id i, function_type m): id (i), migrate (m) {}

    database_id id;
    function_type migrate;
  };
  typedef vector<data_function> data_functions;
  typedef map<data_key, data_functions> data_map;

  struct schema_catalog_impl
  {
    schema_map schema;
    data_map data;
  };

  // Static initialization.
  //
  schema_catalog_impl* schema_catalog_init::catalog = 0;
  size_t schema_catalog_init::count = 0;

  struct schema_catalog_init_extra
  {
    bool initialized;

    schema_catalog_init_extra (): initialized (false) {}
    ~schema_catalog_init_extra ()
    {
      if (initialized && --schema_catalog_init::count == 0)
        delete schema_catalog_init::catalog;
    }
  };

  static schema_catalog_init_extra schema_catalog_init_extra_;

  bool schema_catalog::
  exists (database_id id, const string& name)
  {
    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    return c.schema.find (key (id, name)) != c.schema.end ();
  }

  void schema_catalog::
  create_schema (database& db, const string& name, bool drop)
  {
    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    schema_map::const_iterator i (c.schema.find (key (db.id (), name)));

    if (i == c.schema.end ())
      throw unknown_schema (name);

    const create_functions& fs (i->second.create);

    if (drop)
      drop_schema (db, name);

    // Run the passes until we ran them all or all the functions
    // return false, which means no more passes necessary.
    //
    for (unsigned short pass (1); pass < 3; ++pass)
    {
      bool done (true);

      for (create_functions::const_iterator j (fs.begin ()), e (fs.end ());
           j != e; ++j)
      {
        if ((*j) (db, pass, false))
          done = false;
      }

      if (done)
        break;
    }
  }

  void schema_catalog::
  drop_schema (database& db, const string& name)
  {
    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    schema_map::const_iterator i (c.schema.find (key (db.id (), name)));

    if (i == c.schema.end ())
      throw unknown_schema (name);

    const create_functions& fs (i->second.create);

    // Run the passes until we ran them all or all the functions
    // return false, which means no more passes necessary.
    //
    for (unsigned short pass (1); pass < 3; ++pass)
    {
      bool done (true);

      for (create_functions::const_iterator j (fs.begin ()), e (fs.end ());
           j != e; ++j)
      {
        if ((*j) (db, pass, true))
          done = false;
      }

      if (done)
        break;
    }
  }

  void schema_catalog::
  migrate_schema_impl (database& db,
                       schema_version v,
                       const string& name,
                       migrate_mode m)
  {
    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    schema_map::const_iterator i (c.schema.find (key (db.id (), name)));

    if (i == c.schema.end ())
      throw unknown_schema (name);

    const version_map& vm (i->second.migrate);
    version_map::const_iterator j (vm.find (v));

    if (j == vm.end ())
      throw unknown_schema_version (v);

    const migrate_functions& fs (j->second);

    // Run the passes until we ran them all or all the functions
    // return false, which means no more passes necessary.
    //
    for (bool pre (m != migrate_post);; pre = false)
    {
      for (unsigned short pass (1); pass < 3; ++pass)
      {
        bool done (true);

        for (migrate_functions::const_iterator i (fs.begin ()), e (fs.end ());
             i != e; ++i)
        {
          if ((*i) (db, pass, pre))
            done = false;
        }

        if (done)
          break;
      }

      if (!pre || m != migrate_both)
        break;
    }

    // Update the schema version on the database instance.
    //
    db.schema_version (v, m == migrate_pre, name);
  }

  void schema_catalog::
  migrate_data (database& db, schema_version v, const string& name)
  {
    if (v == 0)
    {
      if (!db.schema_migration ())
        return;

      v = db.schema_version ();
    }

    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    data_map::const_iterator i (c.data.find (data_key (name, v)));

    if (i == c.data.end ())
      return; // No data migration for this schema/version.

    const data_functions& df (i->second);

    for (data_functions::const_iterator i (df.begin ()), e (df.end ());
         i != e; ++i)
    {
      if (i->id == id_common || i->id == db.id ())
        i->migrate (db);
    }
  }

  void schema_catalog::
  data_migration_function (database_id id,
                           schema_version v,
                           data_migration_function_type f,
                           const string& name)
  {
    // This function can be called from a static initializer in which
    // case the catalog might not have yet been created.
    //
    if (schema_catalog_init::count == 0)
    {
      schema_catalog_init::catalog = new schema_catalog_impl;
      ++schema_catalog_init::count;
      schema_catalog_init_extra_.initialized = true;
    }

    schema_catalog_impl& c (*schema_catalog_init::catalog);
    c.data[data_key (name, v)].push_back (data_function (id, f));
  }

  void schema_catalog::
  migrate (database& db, schema_version v, const string& name)
  {
    schema_version latest (latest_version (db, name));

    if (v == 0)
      v = latest;
    else if (v > latest)
      throw unknown_schema_version (v);

    for (schema_version i (next_version (db, 0, name));
         i != 0 && i <= v;
         i = next_version (db, i, name))
    {
      migrate_schema_pre (db, i, name);
      migrate_data (db, i, name);
      migrate_schema_post (db, i, name);
    }
  }

  schema_version schema_catalog::
  next_version (database_id id, schema_version current, const string& name)
  {
    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    schema_map::const_iterator i (c.schema.find (key (id, name)));

    if (i == c.schema.end ())
      throw unknown_schema (name);

    const version_map& vm (i->second.migrate);
    version_map::const_iterator j (vm.upper_bound (current));
    return j != vm.end () ? j->first : 0;
  }

  schema_version schema_catalog::
  latest_version (database_id id, const string& name)
  {
    const schema_catalog_impl& c (*schema_catalog_init::catalog);
    schema_map::const_iterator i (c.schema.find (key (id, name)));

    if (i == c.schema.end ())
      throw unknown_schema (name);

    const version_map& vm (i->second.migrate);
    assert (!vm.empty ());
    return vm.rbegin ()->first;
  }

  // schema_catalog_init
  //
  schema_catalog_init::
  schema_catalog_init ()
  {
    if (count == 0)
      catalog = new schema_catalog_impl;

    ++count;
  }

  schema_catalog_init::
  ~schema_catalog_init ()
  {
    if (--count == 0)
      delete catalog;
  }

  // schema_catalog_create_entry
  //
  schema_catalog_create_entry::
  schema_catalog_create_entry (database_id id,
                               const char* name,
                               create_function cf)
  {
    schema_catalog_impl& c (*schema_catalog_init::catalog);
    c.schema[key(id, name)].create.push_back (cf);
  }

  // schema_catalog_migrate_entry
  //
  schema_catalog_migrate_entry::
  schema_catalog_migrate_entry (database_id id,
                                const char* name,
                                schema_version v,
                                migrate_function mf)
  {
    schema_catalog_impl& c (*schema_catalog_init::catalog);
    c.schema[key(id, name)].migrate[v].push_back (mf);
  }
}
