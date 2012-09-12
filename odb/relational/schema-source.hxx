// file      : odb/relational/schema-source.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_SCHEMA_SOURCE_HXX
#define ODB_RELATIONAL_SCHEMA_SOURCE_HXX

#include <odb/diagnostics.hxx>

#include <odb/relational/context.hxx>
#include <odb/relational/schema.hxx>

namespace relational
{
  namespace schema_source
  {
    struct class_: traversal::class_, virtual context
    {
      typedef class_ base;

      virtual void
      traverse (type& c)
      {
        if (!options.at_once () && class_file (c) != unit.file ())
          return;

        if (!object (c))
          return;

        if (abstract (c) && !polymorphic (c))
          return;

        os << "// " << class_name (c) << endl
           << "//" << endl
           << endl;

        schema_->traverse (c);
      }

    private:
      instance<schema::cxx_object> schema_;
    };

    struct include: virtual context
    {
      typedef include base;

      virtual void
      generate ()
      {
        extra_pre ();
        os << "#include <odb/schema-catalog-impl.hxx>" << endl;
        extra_post ();
        os << endl;
      }

      virtual void
      extra_pre ()
      {
      }

      virtual void
      extra_post ()
      {
      }
    };
  }
}

#endif // ODB_RELATIONAL_SCHEMA_SOURCE_HXX
