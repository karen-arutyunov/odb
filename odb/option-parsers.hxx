// file      : odb/option-parsers.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_OPTION_PARSERS_HXX
#define ODB_OPTION_PARSERS_HXX

#include <sstream>

#include <odb/option-types.hxx>
#include <odb/options.hxx>

namespace cli
{
  template <typename V>
  struct parser<database_map<V> >
  {
    static void
    parse (database_map<V>& m, bool& xs, scanner& s)
    {
      typedef database_map<V> map;

      xs = true;
      std::string o (s.next ());

      if (s.more ())
      {
        std::string ov (s.next ());
        std::string::size_type p = ov.find (':');

        if (p != std::string::npos)
        {
          std::string kstr (ov, 0, p);
          std::string vstr (ov, p + 1);

          // See if this prefix resolves to the database name. If not,
          // assume there is no prefix.
          //
          database k;
          std::istringstream ks (kstr);

          if (ks >> k && ks.eof ())
          {
            V v = V ();

            if (!vstr.empty ())
            {
              std::istringstream vs (vstr);

              if (!(vs >> v && vs.eof ()))
                throw invalid_value (o, ov);
            }

            m[k] = v; // Override any old value.
            return;
          }
        }

        // No database prefix is specified which means it applies to
        // all the databases.
        //
        V v = V ();

        if (!ov.empty ())
        {
          std::istringstream vs (ov);

          if (!(vs >> v && vs.eof ()))
            throw invalid_value (o, ov);
        }

        // We don't want to override database-specific values, so use
        // insert().
        //
        m.insert (typename map::value_type (database::common, v));
        m.insert (typename map::value_type (database::mssql, v));
        m.insert (typename map::value_type (database::mysql, v));
        m.insert (typename map::value_type (database::oracle, v));
        m.insert (typename map::value_type (database::pgsql, v));
        m.insert (typename map::value_type (database::sqlite, v));
      }
      else
        throw missing_value (o);
    }
  };
}


#endif // ODB_OPTION_PARSERS_HXX
