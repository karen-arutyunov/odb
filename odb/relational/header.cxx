// file      : odb/relational/header.cxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/header.hxx>
#include <odb/relational/generate.hxx>

using namespace std;

void relational::header::class1::
traverse_object (type& c)
{
  using semantics::data_member;

  data_member* id (id_member (c));
  bool auto_id (id ? id->count ("auto") : false);
  bool base_id (id ? &id->scope () != &c : false); // Comes from base.

  data_member* optimistic (context::optimistic (c));

  type* poly_root (polymorphic (c));
  bool poly (poly_root != 0);
  bool poly_derived (poly && poly_root != &c);
  type* poly_base (poly_derived ? &polymorphic_base (c) : 0);
  data_member* discriminator (poly ? context::discriminator (*poly_root) : 0);

  bool abst (abstract (c));
  bool reuse_abst (abst && !poly);

  string const& type (class_fq_name (c));
  column_count_type const& cc (column_count (c));

  os << "// " << class_name (c) << endl
     << "//" << endl;

  // class_traits
  //
  os << "template <>" << endl
     << "struct class_traits< " << type << " >"
     << "{"
     << "static const class_kind kind = class_object;"
     << "};";

  // pointer_query_columns & query_columns
  //
  if (options.generate_query ())
  {
    // If we don't have object pointers, then also generate
    // query_columns (in this case pointer_query_columns and
    // query_columns are the same and the former inherits from
    // the latter). Otherwise we have to postpone query_columns
    // generation until the second pass to deal with forward-
    // declared objects.
    //
    if (!has_a (c, test_pointer | include_base))
      query_columns_type_->traverse (c);

    pointer_query_columns_type_->traverse (c);
  }

  // object_traits
  //
  os << "template <>" << endl
     << "class access::object_traits< " << type << " >"
     << "{"
     << "public:" << endl;

  object_public_extra_pre (c);

  // object_type & pointer_type
  //
  os << "typedef " << type << " object_type;"
     << "typedef " << c.get<string> ("object-pointer") << " pointer_type;"
     << "typedef odb::pointer_traits<pointer_type> pointer_traits;"
     << endl;

  // polymorphic, root_type, base_type, etc.
  //
  os << "static const bool polymorphic = " << (poly ? "true" : "false") << ";";

  if (poly)
  {
    os << "typedef " << class_fq_name (*poly_root) << " root_type;"
       << "typedef object_traits<root_type> root_traits;";

    if (poly_derived)
    {
      os << "typedef " << class_fq_name (*poly_base) << " base_type;"
         << "typedef object_traits<base_type> base_traits;"
         << "typedef root_traits::discriminator_type discriminator_type;"
         << "typedef odb::polymorphic_concrete_info<root_type> info_type;";

      if (abst)
        os << "typedef odb::polymorphic_abstract_info<root_type> " <<
          "abstract_info_type;";
      else
        os << "typedef odb::polymorphic_entry<object_type> entry_type;";

      // Calculate our hierarchy depth (number of classes).
      //
      size_t depth (polymorphic_depth (c));

      os << "static const std::size_t depth = " << depth << "UL;";
    }
    else
    {
      semantics::names* hint;
      semantics::type& t (utype (*discriminator, hint));

      os << "typedef " << t.fq_name (hint) << " discriminator_type;"
         << endl;

      os << "struct discriminator_image_type"
         << "{";

      discriminator_image_member_->traverse (*discriminator);

      if (optimistic != 0)
        version_image_member_->traverse (*optimistic);

      os << "std::size_t version;"
         << "};";

      os << "typedef odb::polymorphic_map<object_type> map_type;"
         << "typedef odb::polymorphic_concrete_info<object_type> info_type;";

      if (abst)
        os << "typedef odb::polymorphic_abstract_info<object_type> " <<
          "abstract_info_type;";
      else
        os << "typedef odb::polymorphic_entry<object_type> entry_type;";

      os << "static const std::size_t depth = 1UL;";
    }
  }

  os << endl;

  // id_type & id_image_type
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

      if (poly_derived)
        os << "typedef object_traits< " << type << " >:: id_image_type " <<
          "id_image_type;"
           << "static const bool auto_id = false;"
           << endl;
      else
        os << "typedef object_traits< " << type << " >::id_image_type " <<
          "id_image_type;"
           << "static const bool auto_id = object_traits< " << type <<
          " >::auto_id;"
           << endl;
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

      os << "static const bool auto_id = " << (auto_id ? "true;" : "false;")
         << endl;

      os << "struct id_image_type"
         << "{";

      id_image_member_->traverse (*id);

      if (optimistic != 0)
        version_image_member_->traverse (*optimistic);

      os << "std::size_t version;"
         << "};";
    }
  }
  else if (!reuse_abst)
  {
    // Object without id.
    //
    os << "typedef void id_type;"
       << "static const bool auto_id = false;"
       << endl;
  }

  // abstract
  //
  os << "static const bool abstract = " << (abst ? "true" : "false") << ";"
     << endl;

  // Polymorphic map.
  //
  if (poly)
  {
    if (!poly_derived)
      os << "static map_type* map;";

    os << "static const " << (abst ? "abstract_" : "") << "info_type info;"
       << endl;
  }

  // image_type
  //
  image_type_->traverse (c);

  //
  // Containers (abstract and concrete).
  //

  {
    instance<container_traits> t (c);
    t->traverse (c);
  }

  //
  // Functions (abstract and concrete).
  //

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

  if (!poly_derived && id != 0)
  {
    if (options.generate_query ())
      os << "static id_type" << endl
         << "id (const image_type&);"
         << endl;

    if (optimistic != 0)
      os << "static version_type" << endl
         << "version (const image_type&);"
         << endl;
  }

  // discriminator()
  //
  if (poly && !poly_derived)
    os << "static discriminator_type" << endl
       << "discriminator (const image_type&);"
       << endl;

  // grow ()
  //
  if (generate_grow)
  {
    // For derived classes in a polymorphic hierarchy, grow() will
    // check bases up to the specified depth. If one of the base
    // images has grown, then it will increment its version. But
    // the return value only indicates the state of this image,
    // excluding polymorphic bases (in other words, it is possible
    // that one of the bases has grown but this function returns
    // false).
    //
    os << "static bool" << endl
       << "grow (image_type&, " << truncated_vector;

    if (poly_derived)
      os << ", std::size_t = depth";

    os << ");"
       << endl;
  }

  // bind (image_type)
  //
  os << "static void" << endl
     << "bind (" << bind_vector << "," << endl;

  // If we are a derived type in a polymorphic hierarchy, then
  // we get the the external id binding.
  //
  if (poly_derived)
    os << "const " << bind_vector << " id," << endl
       << "std::size_t id_size," << endl;

  os << "image_type&," << endl
     << db << "::statement_kind);"
     << endl;

  // bind (id_image_type)
  //
  if (id != 0)
  {
    os << "static void" << endl
       << "bind (" << bind_vector << ", id_image_type&" <<
      (optimistic != 0 ? ", bool bind_version = true" : "") << ");"
       << endl;
  }

  // init (image, object)
  //
  os << "static " << (generate_grow ? "bool" : "void") << endl
     << "init (image_type&, const object_type&, " << db << "::statement_kind);"
     << endl;

  // init (object, image)
  //
  os << "static void" << endl
     << "init (object_type&, const image_type&, database*";

  if (poly_derived)
    os << ", std::size_t = depth";

  os << ");"
     << endl;

  // init (id_image, id)
  //
  if (id != 0)
  {
    os << "static void" << endl
       << "init (id_image_type&, const id_type&" <<
      (optimistic != 0 ? ", const version_type* = 0" : "") << ");"
       << endl;
  }

  if (poly_derived)
  {
    // check_version
    //
    os << "static bool" << endl
       << "check_version (const std::size_t*, const image_type&);"
       << endl;

    // update_version
    //
    os << "static void" << endl
       << "update_version (std::size_t*, const image_type&, " <<
      db << "::binding*);"
       << endl;
  }

  // The rest does not apply to reuse-abstract objects.
  //
  if (reuse_abst)
  {
    object_public_extra_post (c);
    os << "};";
    return;
  }

  // Cache traits typedefs.
  //
  if (id == 0)
  {
    os << "typedef" << endl
       << "odb::no_id_pointer_cache_traits<pointer_type>" << endl
       << "pointer_cache_traits;"
       << "typedef" << endl
       << "odb::no_id_reference_cache_traits<object_type>" << endl
       << "reference_cache_traits;"
       << endl;
  }
  else
  {
    char const* p (session (c) ? "" : "no_op_");

    if (poly_derived)
    {
      os << "typedef" << endl
         << "odb::" << p << "pointer_cache_traits<" <<
        "root_traits::pointer_type>" << endl
         << "pointer_cache_traits;"
         << "typedef" << endl
         << "odb::" << p << "reference_cache_traits<root_type>" << endl
         << "reference_cache_traits;"
         << endl;
    }
    else
    {
      os << "typedef" << endl
         << "odb::" << p << "pointer_cache_traits<pointer_type>" << endl
         << "pointer_cache_traits;"
         << "typedef" << endl
         << "odb::" << p << "reference_cache_traits<object_type>" << endl
         << "reference_cache_traits;"
         << endl;
    }
  }

  // Statements typedefs.
  //
  if (poly)
  {
    if (poly_derived)
      os << "typedef" << endl
         << db << "::polymorphic_derived_object_statements" <<
        "<object_type>" << endl
         << "statements_type;"
         << "typedef" << endl
         << db << "::polymorphic_root_object_statements<root_type>" << endl
         << "root_statements_type;"
         << endl;
    else
      os << "typedef" << endl
         << db << "::polymorphic_root_object_statements<object_type>" << endl
         << "statements_type;"
         << "typedef statements_type root_statements_type;"
         << "typedef " << db << "::object_statements<object_type> " <<
        "base_statements_type;"
         << endl;
  }
  else
  {
    if (id != 0)
      os << "typedef " << db << "::object_statements<object_type> " <<
        "statements_type;"
         << endl;
    else
      os << "typedef " << db << "::no_id_object_statements<object_type> " <<
        "statements_type;"
         << endl;
  }

  //
  // Query (concrete).
  //

  if (options.generate_query ())
  {
    // query_base_type
    //
    os << "typedef " << db << "::query query_base_type;"
       << endl;

    // query_type
    //
    os << "struct query_type;";
  }

  //
  // Containers (concrete).
  //

  // Statement cache (forward declaration).
  //
  if (id != 0)
    os << "struct container_statement_cache_type;"
       << endl;

  // column_count
  //
  os << "static const std::size_t column_count = " << cc.total << "UL;"
     << "static const std::size_t id_column_count = " << cc.id << "UL;"
     << "static const std::size_t inverse_column_count = " <<
    cc.inverse << "UL;"
     << "static const std::size_t readonly_column_count = " <<
    cc.readonly << "UL;"
     << "static const std::size_t managed_optimistic_column_count = " <<
    cc.optimistic_managed << "UL;";

  if (poly && !poly_derived)
    os << "static const std::size_t discriminator_column_count = " <<
      cc.discriminator << "UL;";

  os << endl;

  // Statements.
  //
  os << "static const char persist_statement[];";

  if (id != 0)
  {
    if (poly_derived)
    {
      char const* n (abst ? "1" : "depth");

      os << "static const char* const find_statements[" << n << "];"
         << "static const std::size_t find_column_counts[" << n << "];";
    }
    else
    {
      os << "static const char find_statement[];";

      if (poly)
        os << "static const char find_discriminator_statement[];";
    }

    if (cc.total != cc.id + cc.inverse + cc.readonly)
      os << "static const char update_statement[];";

    os << "static const char erase_statement[];";

    if (optimistic != 0 && !poly_derived)
      os << "static const char optimistic_erase_statement[];";
  }

  if (options.generate_query ())
  {
    os << "static const char query_statement[];"
       << "static const char erase_query_statement[];"
       << endl
       << "static const char table_name[];";
  }

  os << endl;

  //
  // Functions (concrete).
  //

  // callback ()
  //
  os << "static void" << endl
     << "callback (database&, object_type&, callback_event);"
     <<  endl;

  os << "static void" << endl
     << "callback (database&, const object_type&, callback_event);"
     <<  endl;

  // persist ()
  //
  os << "static void" << endl
     << "persist (database&, " << (auto_id ? "" : "const ") << "object_type&";

  if (poly)
    os << ", bool top = true, bool dyn = true";

  os << ");"
     << endl;

  if (id != 0)
  {
    // find (id)
    //
    if (c.default_ctor ())
      os << "static pointer_type" << endl
         << "find (database&, const id_type&);"
         << endl;

    // find (id, obj)
    //
    os << "static bool" << endl
       << "find (database&, const id_type&, object_type&";

    if (poly)
      os << ", bool dyn = true";

    os << ");"
       << endl;

    // reload ()
    //
    os << "static bool" << endl
       << "reload (database&, object_type&";

    if (poly)
      os << ", bool dyn = true";

    os << ");"
       << endl;

    // update ()
    //
    // In case of a polymorphic object, we generate update() even if it is
    // readonly since the potentially-readwrite base will rely on it to
    // initialize the id image.
    //
    //
    if (!readonly (c) || poly)
    {
      os << "static void" << endl
         << "update (database&, const object_type&";

      if (poly)
        os << ", bool top = true, bool dyn = true";

      os << ");"
         << endl;
    }

    // erase ()
    //
    os << "static void" << endl
       << "erase (database&, const id_type&";

    if (poly)
      os << ", bool top = true, bool dyn = true";

    os << ");"
       << endl;

    os << "static void" << endl
       << "erase (database&, const object_type&";

    if (poly)
      os << ", bool top = true, bool dyn = true";

    os << ");"
       << endl;
  }

  // query ()
  //
  if (options.generate_query ())
  {
    os << "static result<object_type>" << endl
       << "query (database&, const query_base_type&);"
       << endl;

    os << "static unsigned long long" << endl
       << "erase_query (database&, const query_base_type&);"
       << endl;
  }

  // create_schema ()
  //
  if (embedded_schema || separate_schema)
  {
    os << "static bool" << endl
       << "create_schema (database&, unsigned short pass, bool drop);"
       << endl;
  }

  object_public_extra_post (c);

  // Implementation details.
  //
  os << "public:" << endl;

  if (id != 0)
  {
    // Load the object image.
    //
    os << "static bool" << endl
       << "find_ (";

    if (poly && !poly_derived)
      os << "base_statements_type&, ";
    else
      os << "statements_type&, ";

    os << "const id_type*";

    if (poly_derived && !abst)
      os << ", std::size_t = depth";

    os << ");"
       << endl;

    // Load the rest of the object (containers, etc). Expects the id
    // image in the object statements to be initialized to the object
    // id.
    //
    os << "static void" << endl
       << "load_ (";

    if (poly && !poly_derived)
      os << "base_statements_type&, ";
    else
      os << "statements_type&, ";

    os << "object_type&";

    if (poly_derived)
      os << ", std::size_t = depth";

    os << ");"
       << endl;
  }

  // discriminator_ ()
  //
  if (poly && !poly_derived)
  {
    os << "static void" << endl
       << "discriminator_ (statements_type&," << endl
       << "const id_type&," << endl
       << "discriminator_type*";

    if (optimistic != 0)
      os << "," << endl
         << "version_type* = 0";

    os << ");"
       << endl;
  }

  // Load the dynamic part of the object. Depth inidicates where
  // the dynamic part starts. Expects the id image in the object
  // statements to be initialized to the object id. We don't need
  // it if we are poly-abstract.
  //
  if (poly_derived && !abst)
    os << "static void" << endl
       << "load_ (database&, root_type&, std::size_t);"
       << endl;

  // Image chain manipulation.
  //
  if (poly && need_image_clone && options.generate_query ())
  {
    os << "static root_traits::image_type&" << endl
       << "root_image (image_type&);"
       << endl;

    os << "static image_type*" << endl
       << "clone_image (const image_type&);"
       << endl;

    os << "static void" << endl
       << "copy_image (image_type&, const image_type&);"
       << endl;

    os << "static void" << endl
       << "free_image (image_type*);"
       << endl;
  }

  os << "};";
}

void relational::header::
generate ()
{
  context ctx;
  ostream& os (ctx.os);

  instance<include> i;
  i->generate ();

  os << "namespace odb"
     << "{";

  {
    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (false);
    traversal::namespace_ ns;
    instance<class1> c;

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (false);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

    unit.dispatch (ctx.unit);
  }

  {
    traversal::unit unit;
    traversal::defines unit_defines;
    typedefs unit_typedefs (false);
    traversal::namespace_ ns;
    instance<class2> c;

    unit >> unit_defines >> ns;
    unit_defines >> c;
    unit >> unit_typedefs >> c;

    traversal::defines ns_defines;
    typedefs ns_typedefs (false);

    ns >> ns_defines >> ns;
    ns_defines >> c;
    ns >> ns_typedefs >> c;

    unit.dispatch (ctx.unit);
  }

  os << "}";
}
