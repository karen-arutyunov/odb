// file      : odb/relational/inline.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_INLINE_HXX
#define ODB_RELATIONAL_INLINE_HXX

#include <odb/relational/context.hxx>
#include <odb/relational/common.hxx>

namespace relational
{
  namespace inline_
  {
    //
    //
    struct callback_calls: traversal::class_, virtual context
    {
      typedef callback_calls base;

      callback_calls ()
      {
        *this >> inherits_ >> *this;
      }

      callback_calls (callback_calls const&)
          : root_context (), //@@ -Wextra
            context ()
      {
        *this >> inherits_ >> *this;
      }

      virtual void
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
          // there is a const callback.
          //
          if (const_)
          {
            if (c.count ("callback-const"))
              os << "static_cast< const " << c.fq_name () << "& > (x)." <<
                name << " (e, db);";
          }
          else
            os << "static_cast< " << c.fq_name () << "& > (x)." <<
              name << " (e, db);";
        }
        else if (obj)
          inherits (c);
      }

    protected:
      bool const_;
      traversal::inherits inherits_;
    };

    //
    //
    struct class_: traversal::class_, virtual context
    {
      typedef class_ base;

      virtual void
      traverse (type& c)
      {
        if (c.file () != unit.file ())
          return;

        if (object (c))
          traverse_object (c);
        if (view (c))
          traverse_view (c);
        else if (composite (c))
          traverse_composite (c);
      }

      virtual void
      object_extra (type&)
      {
      }

      virtual void
      traverse_object (type& c)
      {
        bool abst (abstract (c));
        string const& type (c.fq_name ());
        string traits ("access::object_traits< " + type + " >");

        semantics::data_member* id (id_member (c));
        bool base_id (id ? &id->scope () != &c : false); // Comes from base.

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        object_extra (c);

        if (id != 0)
        {
          // id (object_type)
          //
          os << "inline" << endl
             << traits << "::id_type" << endl
             << traits << "::" << endl
             << "id (const object_type& obj)"
             << "{";

          if (base_id)
            os << "return object_traits< " << id->scope ().fq_name () <<
              " >::id (obj);";
          else
            os << "return obj." << id->name () << ";";

          os << "}";

          // id (image_type)
          //
          if (options.generate_query () && base_id)
          {
            os << "inline" << endl
               << traits << "::id_type" << endl
               << traits << "::" << endl
               << "id (const image_type& i)"
               << "{"
               << "return object_traits< " << id->scope ().fq_name () <<
              " >::id (i);"
               << "}";
          }

          // bind (id_image_type)
          //
          if (base_id)
          {
            os << "inline" << endl
               << "void " << traits << "::" << endl
               << "bind (" << bind_vector << " b, id_image_type& i)"
               << "{"
               << "object_traits< " << id->scope ().fq_name () <<
              " >::bind (b, i);"
               << "}";
          }

          if (base_id)
          {
            os << "inline" << endl
               << "void " << traits << "::" << endl
               << "init (id_image_type& i, const id_type& id)"
               << "{"
               << "object_traits< " << id->scope ().fq_name () <<
              " >::init (i, id);"
               << "}";
          }
        }

        //
        // The rest only applies to concrete objects.
        //
        if (abst)
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
        callback_calls_->traverse (c, false);
        os << "}";

        os << "inline" << endl
           << "void " << traits << "::" << endl
           << "callback (database& db, const object_type& x, callback_event e)"
           << "{"
           << "ODB_POTENTIALLY_UNUSED (db);"
           << "ODB_POTENTIALLY_UNUSED (x);"
           << "ODB_POTENTIALLY_UNUSED (e);"
           << endl;
        callback_calls_->traverse (c, true);
        os << "}";

        // query_type
        //
        if (options.generate_query ())
        {
          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type ()"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (bool v)" << endl
             << "  : query_base_type (v)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const char* q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const std::string& q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const query_base_type& q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";
        }

        // load_()
        //
        if (!has_a (c, test_container))
        {
          os << "inline" << endl
             << "void " << traits << "::" << endl
             << "load_ (" << db << "::object_statements< object_type >&, " <<
            "object_type&)"
             << "{"
             << "}";
        }
      }

      virtual void
      view_extra (type&)
      {
      }

      virtual void
      traverse_view (type& c)
      {
        string const& type (c.fq_name ());
        string traits ("access::view_traits< " + type + " >");

        os << "// " << c.name () << endl
           << "//" << endl
           << endl;

        view_extra (c);

        // query_type
        //
        if (c.count ("objects"))
        {
          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type ()"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (bool v)" << endl
             << "  : query_base_type (v)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const char* q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const std::string& q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";

          os << "inline" << endl
             << traits << "::query_type::" << endl
             << "query_type (const query_base_type& q)" << endl
             << "  : query_base_type (q)"
             << "{"
             << "}";
        }

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
        callback_calls_->traverse (c, false);
        os << "}";
      }

      virtual void
      traverse_composite (type&)
      {
        /*
          string const& type (c.fq_name ());
          string traits ("access::composite_value_traits< " + type + " >");

          os << "// " << c.name () << endl
          << "//" << endl
          << endl;

        */
      }

    private:
      instance<callback_calls> callback_calls_;
    };
  }
}

#endif // ODB_RELATIONAL_INLINE_HXX
