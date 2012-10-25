// file      : odb/header.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/common.hxx>
#include <odb/context.hxx>
#include <odb/generate.hxx>

using namespace std;

namespace header
{
  struct class_: traversal::class_, virtual context
  {
    virtual void
    traverse (type& c)
    {
      if (!options.at_once () && class_file (c) != unit.file ())
        return;

      if (object (c))
        traverse_object (c);
      else if (view (c))
        traverse_view (c);
    }

    void
    traverse_object (type&);

    void
    traverse_view (type&);

    void
    traverse_composite (type&);
  };
}

void header::class_::
traverse_object (type& c)
{
  using semantics::data_member;

  data_member* id (id_member (c));
  bool auto_id (id && auto_ (*id));
  bool base_id (id && &id->scope () != &c); // Comes from base.

  data_member* optimistic (context::optimistic (c));

  type* poly_root (polymorphic (c));
  bool poly (poly_root != 0);
  bool poly_derived (poly && poly_root != &c);
  type* poly_base (poly_derived ? &polymorphic_base (c) : 0);
  data_member* discriminator (poly ? context::discriminator (*poly_root) : 0);

  bool abst (abstract (c));
  bool reuse_abst (abst && !poly);

  string const& type (class_fq_name (c));

  os << "// " << class_name (c) << endl
     << "//" << endl;

  // class_traits
  //
  os << "template <>" << endl
     << "struct class_traits< " << type << " >"
     << "{"
     << "static const class_kind kind = class_object;"
     << "};";

  // object_traits
  //
  os << "template <>" << endl
     << "class access::object_traits< " << type << " >"
     << "{"
     << "public:" << endl;

  // object_type & pointer_type
  //
  os << "typedef " << type << " object_type;"
     << "typedef " << c.get<string> ("object-pointer") << " pointer_type;"
     << "typedef odb::pointer_traits<pointer_type> pointer_traits;"
     << endl;

  // polymorphic, root_type, base_type, etc.
  //
  os << "static const bool polymorphic = " << (poly ? "true" : "false") << ";"
     << endl;

  if (poly)
  {
    os << "typedef " << class_fq_name (*poly_root) << " root_type;";

    if (poly_derived)
    {
      os << "typedef " << class_fq_name (*poly_base) << " base_type;"
         << "typedef object_traits<root_type>::discriminator_type " <<
        "discriminator_type;"
         << "typedef polymorphic_concrete_info<root_type> info_type;";

      if (abst)
        os << "typedef polymorphic_abstract_info<root_type> " <<
          "abstract_info_type;";

      // Calculate our hierarchy depth (number of classes).
      //
      size_t depth (polymorphic_depth (c));

      os << endl
         << "static const std::size_t depth = " << depth << "UL;";
    }
    else
    {
      semantics::names* hint;
      semantics::type& t (utype (*discriminator, hint));

      os << "typedef " << t.fq_name (hint) << " discriminator_type;"
         << "typedef polymorphic_map<object_type> map_type;"
         << "typedef polymorphic_concrete_info<object_type> info_type;";

      if (abst)
        os << "typedef polymorphic_abstract_info<object_type> " <<
          "abstract_info_type;";

      os << endl
         << "static const std::size_t depth = 1UL;";
    }

    os << endl;
  }

  // id_type, version_type, etc.
  //
  if (id != 0)
  {
    if (base_id)
    {
      semantics::class_& b (
        dynamic_cast<semantics::class_&> (id->scope ()));
      string const& type (class_fq_name (b));

      os << "typedef object_traits< " << type << " >::id_type id_type;";

      if (optimistic != 0)
        os << "typedef object_traits< " << type << " >::version_type " <<
          "version_type;";

      os << endl;

      if (poly_derived)
        os << "static const bool auto_id = false;";
      else
        os << "static const bool auto_id = object_traits< " << type <<
          " >::auto_id;";
    }
    else
    {
      {
        semantics::names* hint;
        semantics::type& t (utype (*id, hint));
        os << "typedef " << t.fq_name (hint) << " id_type;";
      }

      if (optimistic != 0)
      {
        semantics::names* hint;
        semantics::type& t (utype (*optimistic, hint));
        os << "typedef " << t.fq_name (hint) << " version_type;";
      }

      os << endl
         << "static const bool auto_id = " << (auto_id ? "true;" : "false;");
    }

    os << endl;
  }
  else if (!reuse_abst)
  {
    // Object without id.
    //
    os << "typedef void id_type;"
       << endl
       << "static const bool auto_id = false;"
       << endl;
  }

  // abstract
  //
  os << "static const bool abstract = " << (abst ? "true" : "false") << ";"
     << endl;

  // id ()
  //
  if (id != 0 || !reuse_abst)
  {
    // We want to generate a dummy void id() accessor even if this
    // object has no id to help us in the runtime. This way we can
    // write generic code that will work for both void and non-void
    // ids.
    //
    os << "static id_type" << endl
       << "id (const object_type&);"
       << endl;
  }

  if (!reuse_abst)
  {
    // Cache traits typedefs.
    //
    if (id == 0)
    {
      os << "typedef" << endl
         << "no_id_pointer_cache_traits<pointer_type>" << endl
         << "pointer_cache_traits;"
         << endl
         << "typedef" << endl
         << "no_id_reference_cache_traits<object_type>" << endl
         << "reference_cache_traits;"
         << endl;
    }
    else
    {
      char const* p (session (c) ? "odb::" : "no_op_");

      if (poly_derived)
      {
        os << "typedef" << endl
           << p << "pointer_cache_traits<" <<
          "object_traits<root_type>::pointer_type>" << endl
           << "pointer_cache_traits;"
           << endl
           << "typedef" << endl
           << p << "reference_cache_traits<root_type>" << endl
           << "reference_cache_traits;"
           << endl;
      }
      else
      {
        os << "typedef" << endl
           << p << "pointer_cache_traits<pointer_type>" << endl
           << "pointer_cache_traits;"
           << endl
           << "typedef" << endl
           << p << "reference_cache_traits<object_type>" << endl
           << "reference_cache_traits;"
           << endl;
      }
    }

    // callback ()
    //
    os << "static void" << endl
       << "callback (database&, object_type&, callback_event);"
       <<  endl;

    os << "static void" << endl
       << "callback (database&, const object_type&, callback_event);"
       <<  endl;
  }

  os << "};";
}

void header::class_::
traverse_view (type& c)
{
  string const& type (class_fq_name (c));

  os << "// " << class_name (c) << endl
     << "//" << endl;

  // class_traits
  //
  os << "template <>" << endl
     << "struct class_traits< " << type << " >"
     << "{"
     << "static const class_kind kind = class_view;"
     << "};";

  // view_traits
  //
  os << "template <>" << endl
     << "class access::view_traits< " << type << " >"
     << "{"
     << "public:" << endl;

  // view_type & pointer_type
  //
  os << "typedef " << type << " view_type;"
     << "typedef " << c.get<string> ("object-pointer") << " pointer_type;"
     << endl;

  // callback ()
  //
  os << "static void" << endl
     << "callback (database&, view_type&, callback_event);"
     <<  endl;

  os << "};";
}

namespace header
{
  void
  generate ()
  {
    context ctx;
    ostream& os (ctx.os);

    os << "#include <memory>" << endl
       << "#include <cstddef>" << endl; // std::size_t

    if (ctx.features.polymorphic_object)
      os << "#include <string>" << endl; // For discriminator.

    os << endl;

    os << "#include <odb/core.hxx>" << endl
       << "#include <odb/traits.hxx>" << endl
       << "#include <odb/callback.hxx>" << endl
       << "#include <odb/wrapper-traits.hxx>" << endl
       << "#include <odb/pointer-traits.hxx>" << endl;

    // In case of a boost TR1 implementation, we cannot distinguish
    // between the boost::shared_ptr and std::tr1::shared_ptr usage since
    // the latter is just a using-declaration for the former. To resolve
    // this we will include TR1 traits if the Boost TR1 header is included.
    //
    if (ctx.features.tr1_pointer)
    {
      os << "#include <odb/tr1/wrapper-traits.hxx>" << endl
         << "#include <odb/tr1/pointer-traits.hxx>" << endl;
    }
    else if (ctx.features.boost_pointer)
    {
      os << "#ifdef BOOST_TR1_MEMORY_HPP_INCLUDED" << endl
         << "#  include <odb/tr1/wrapper-traits.hxx>" << endl
         << "#  include <odb/tr1/pointer-traits.hxx>" << endl
         << "#endif" << endl;
    }

    os << "#include <odb/container-traits.hxx>" << endl;

    if (ctx.features.session_object)
      os << "#include <odb/cache-traits.hxx>" << endl;
    else
      os << "#include <odb/no-op-cache-traits.hxx>" << endl;

    if (ctx.features.polymorphic_object)
      os << "#include <odb/polymorphic-info.hxx>" << endl;

    if (ctx.options.generate_query ())
    {
      if (ctx.options.generate_prepared ())
        os << "#include <odb/prepared-query.hxx>" << endl;

      os << "#include <odb/result.hxx>" << endl;

      if (ctx.features.simple_object)
        os << "#include <odb/simple-object-result.hxx>" << endl;

      if (ctx.features.polymorphic_object)
        os << "#include <odb/polymorphic-object-result.hxx>" << endl;

      if (ctx.features.no_id_object)
        os << "#include <odb/no-id-object-result.hxx>" << endl;

      if (ctx.features.view)
        os << "#include <odb/view-result.hxx>" << endl;
    }

    os << endl
       << "#include <odb/details/unused.hxx>" << endl
       << endl;

    // Generate common code.
    //
    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (false);
    traversal::namespace_ ns;
    class_ c;

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (false);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

    os << "namespace odb"
       << "{";

    unit.dispatch (ctx.unit);

    os << "}";
  }
}
