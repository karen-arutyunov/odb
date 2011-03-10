// file      : odb/relational/mysql/schema.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/schema.hxx>

#include <odb/relational/mysql/common.hxx>
#include <odb/relational/mysql/context.hxx>

namespace relational
{
  namespace mysql
  {
    namespace schema
    {
      namespace relational = relational::schema;

      //
      // Create.
      //

      struct create_common: virtual relational::create_common
      {
        virtual void
        create_post ()
        {
          os << ")";

          string const& engine (options.mysql_engine ());

          if (engine != "default")
            os << endl
               << " ENGINE=" << engine;

          os << endl;
        }
      };

      struct object_columns: relational::object_columns
      {
        object_columns (base const& x): base (x) {}

        virtual void
        constraints (semantics::data_member& m)
        {
          base::constraints (m);

          if (m.count ("auto"))
            os << " AUTO_INCREMENT";
        }

      };
      entry<object_columns> object_columns_;

      struct member_create: relational::member_create, create_common
      {
        member_create (base const& x): base (x) {}
      };
      entry<member_create> member_create_;

      struct class_create: relational::class_create, create_common
      {
        class_create (base const& x): base (x) {}
      };
      entry<class_create> class_create_;
    }
  }
}
