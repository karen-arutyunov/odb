// file      : odb/relational/sqlite/source.cxx
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <odb/relational/source.hxx>

#include <odb/relational/sqlite/common.hxx>
#include <odb/relational/sqlite/context.hxx>

using namespace std;

namespace relational
{
  namespace sqlite
  {
    namespace source
    {
      namespace relational = relational::source;

      //
      // bind
      //

      struct bind_member: relational::bind_member, member_base
      {
        bind_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi))
            return false;

          ostringstream ostr;
          ostr << "b[n]";
          b = ostr.str ();

          arg = arg_override_.empty () ? string ("i") : arg_override_;

          if (var_override_.empty ())
          {
            os << "// " << mi.m.name () << endl
               << "//" << endl;

            if (inverse (mi.m, key_prefix_) || version (mi.m))
              os << "if (sk == statement_select)"
                 << "{";
            // If the whole class is readonly, then we will never be
            // called with sk == statement_update.
            //
            else if (!readonly (*context::top_object))
            {
              semantics::class_* c;

              if (id (mi.m) ||
                  readonly (mi.m) ||
                  ((c = composite (mi.t)) && readonly (*c)))
                os << "if (sk != statement_update)"
                   << "{";
            }
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (var_override_.empty ())
          {
            semantics::class_* c;

            if ((c = composite (mi.t)))
            {
              bool ro (readonly (*c));
              column_count_type const& cc (column_count (*c));

              os << "n += " << cc.total << "UL";

              // select = total
              // insert = total - inverse
              // update = total - inverse - readonly
              //
              if (cc.inverse != 0 || (!ro && cc.readonly != 0))
              {
                os << " - (" << endl
                   << "sk == statement_select ? 0 : ";

                if (cc.inverse != 0)
                  os << cc.inverse << "UL";

                if (!ro && cc.readonly != 0)
                {
                  if (cc.inverse != 0)
                    os << " + ";

                  os << "(" << endl
                     << "sk == statement_insert ? 0 : " <<
                    cc.readonly << "UL)";
                }

                os << ")";
              }

              os << ";";
            }
            else
              os << "n++;";

            bool block (false);

            // The same logic as in pre().
            //
            if (inverse (mi.m, key_prefix_) || version (mi.m))
              block = true;
            else if (!readonly (*context::top_object))
            {
              semantics::class_* c;

              if (id (mi.m) ||
                  readonly (mi.m) ||
                  ((c = composite (mi.t)) && readonly (*c)))
                block = true;
            }

            if (block)
              os << "}";
            else
              os << endl;
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "composite_value_traits< " << mi.fq_type () <<
            " >::bind (b + n, " << arg << "." << mi.var << "value, sk);";
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << b << ".type = sqlite::bind::integer;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_real (member_info& mi)
        {
          os << b << ".type = sqlite::bind::real;"
             << b << ".buffer = &" << arg << "." << mi.var << "value;"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_text (member_info& mi)
        {
          os << b << ".type = sqlite::bind::text;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".capacity = " << arg << "." << mi.var <<
            "value.capacity ();"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

        virtual void
        traverse_blob (member_info& mi)
        {
          os << b << ".type = sqlite::bind::blob;"
             << b << ".buffer = " << arg << "." << mi.var << "value.data ();"
             << b << ".size = &" << arg << "." << mi.var << "size;"
             << b << ".capacity = " << arg << "." << mi.var <<
            "value.capacity ();"
             << b << ".is_null = &" << arg << "." << mi.var << "null;";
        }

      private:
        string b;
        string arg;
      };
      entry<bind_member> bind_member_;

      //
      // grow
      //

      struct grow_member: relational::grow_member, member_base
      {
        grow_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi))
            return false;

          ostringstream ostr;
          ostr << "t[" << index_ << "UL]";
          e = ostr.str ();

          if (var_override_.empty ())
            os << "// " << mi.m.name () << endl
               << "//" << endl;

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (semantics::class_* c = composite (mi.t))
            index_ += column_count (*c).total;
          else
            index_++;
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "if (composite_value_traits< " << mi.fq_type () <<
            " >::grow (" << endl
             << "i." << mi.var << "value, t + " << index_ << "UL))"
             << "{"
             << "grew = true;"
             << "}";
        }

        virtual void
        traverse_integer (member_info&)
        {
          os << e << " = false;"
             << endl;
        }

        virtual void
        traverse_real (member_info&)
        {
          os << e << " = false;"
             << endl;
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << "if (" << e << ")" << endl
             << "{"
             << "i." << mi.var << "value.capacity (i." << mi.var << "size);"
             << "grew = true;"
             << "}";
        }

      private:
        string e;
      };
      entry<grow_member> grow_member_;

      //
      // init image
      //

      struct init_image_member: relational::init_image_member, member_base
      {
        init_image_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x),
              member_database_type_id_ (base::type_override_,
                                        base::fq_type_override_,
                                        base::key_prefix_)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          // Ignore containers (they get their own table) and inverse
          // object pointers (they are not present in this binding).
          //
          if (container (mi) || inverse (mi.m, key_prefix_))
            return false;

          if (!member_override_.empty ())
            member = member_override_;
          else
          {
            // If we are generating standard init() and this member
            // contains version, ignore it.
            //
            if (version (mi.m))
              return false;

            string const& name (mi.m.name ());
            member = "o." + name;

            os << "// " << name << endl
               << "//" << endl;

            // If the whole class is readonly, then we will never be
            // called with sk == statement_update.
            //
            if (!readonly (*context::top_object))
            {
              semantics::class_* c;

              if (id (mi.m) ||
                  readonly (mi.m) ||
                  ((c = composite (mi.t)) && readonly (*c)))
                os << "if (sk == statement_insert)";
            }
          }

          // If this is a wrapped composite value, then we need to
          // "unwrap" it. For simple values this is taken care of
          // by the value_traits specializations.
          //
          if (mi.wrapper != 0 && composite (mi.t))
          {
            // Here we need the wrapper type, not the wrapped type.
            //
            member = "wrapper_traits< " + mi.fq_type (false) + " >::" +
              "get_ref (" + member + ")";
          }

          if (composite (mi.t))
          {
            os << "{";
            traits = "composite_value_traits< " + mi.fq_type () + " >";
          }
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_utype (mi.m, key_prefix_));

            if (semantics::class_* c = object_pointer (mt))
            {
              type = "obj_traits::id_type";
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              // Handle NULL pointers and extract the id.
              //
              os << "{"
                 << "typedef object_traits< " << class_fq_name (*c) <<
                " > obj_traits;";

              if (weak_pointer (mt))
              {
                os << "typedef pointer_traits< " << mi.fq_type () <<
                  " > wptr_traits;"
                   << "typedef pointer_traits< wptr_traits::" <<
                  "strong_pointer_type > ptr_traits;"
                   << endl
                   << "wptr_traits::strong_pointer_type sp (" <<
                  "wptr_traits::lock (" << member << "));";

                member = "sp";
              }
              else
                os << "typedef pointer_traits< " << mi.fq_type () <<
                  " > ptr_traits;"
                   << endl;

              os << "i." << mi.var << "null = ptr_traits::null_ptr (" <<
                member << ");"
                 << "if (!i." << mi.var << "null)"
                 << "{"
                 << "const " << type << "& id (" << endl;

              if (lazy_pointer (mt))
                os << "ptr_traits::object_id< ptr_traits::element_type  > (" <<
                  member << ")";
              else
                os << "obj_traits::id (ptr_traits::get_ref (" << member << "))";

              os << ");"
                 << endl;

              member = "id";
            }
            else
            {
              type = mi.fq_type ();
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              os << "{";
            }

            traits = "sqlite::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (composite (mi.t))
            os << "}";
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            if (object_pointer (member_utype (mi.m, key_prefix_)))
            {
              os << "}";

              if (!null (mi.m, key_prefix_))
                os << "else" << endl
                   << "throw null_pointer ();";
            }

            os << "}";
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << "if (" << traits << "::init (" << endl
             << "i." << mi.var << "value," << endl
             << member << "," << endl
             << "sk))" << endl
             << "grew = true;";
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null," << endl
             << member << ");";
        }

        virtual void
        traverse_real (member_info& mi)
        {
          os << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null," << endl
             << member << ");";
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << "std::size_t cap (i." << mi.var << "value.capacity ());"
             << traits << "::set_image (" << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "null," << endl
             << member << ");"
             << "grew = grew || (cap != i." << mi.var << "value.capacity ());";
        }

      private:
        string type;
        string db_type_id;
        string member;
        string traits;

        member_database_type_id member_database_type_id_;
      };
      entry<init_image_member> init_image_member_;

      //
      // init value
      //

      struct init_value_member: relational::init_value_member, member_base
      {
        init_value_member (base const& x)
            : member_base::base (x), // virtual base
              base (x),
              member_base (x),
              member_database_type_id_ (base::type_override_,
                                        base::fq_type_override_,
                                        base::key_prefix_)
        {
        }

        virtual bool
        pre (member_info& mi)
        {
          if (container (mi))
            return false;

          if (!member_override_.empty ())
            member = member_override_;
          else
          {
            string const& name (mi.m.name ());
            member = "o." + name;

            if (mi.cq)
              member = "const_cast< " + mi.fq_type (false) + "& > (" +
                member + ")";

            os << "// " << name << endl
               << "//" << endl;
          }

          // If this is a wrapped composite value, then we need to
          // "unwrap" it. For simple values this is taken care of
          // by the value_traits specializations.
          //
          if (mi.wrapper != 0 && composite (mi.t))
          {
            // Here we need the wrapper type, not the wrapped type.
            //
            member = "wrapper_traits< " + mi.fq_type (false) + " >::" +
              "set_ref (\n" + member + ")";
          }

          if (composite (mi.t))
            traits = "composite_value_traits< " + mi.fq_type () + " >";
          else
          {
            // When handling a pointer, mi.t is the id type of the referenced
            // object.
            //
            semantics::type& mt (member_utype (mi.m, key_prefix_));

            if (semantics::class_* c = object_pointer (mt))
            {
              type = "obj_traits::id_type";
              db_type_id = member_database_type_id_.database_type_id (mi.m);

              // Handle NULL pointers and extract the id.
              //
              os << "{"
                 << "typedef object_traits< " << class_fq_name (*c) <<
                " > obj_traits;"
                 << "typedef pointer_traits< " << mi.fq_type () <<
                " > ptr_traits;"
                 << endl
                 << "if (i." << mi.var << "null)" << endl;

              if (null (mi.m, key_prefix_))
                os << member << " = ptr_traits::pointer_type ();";
              else
                os << "throw null_pointer ();";

              os << "else"
                 << "{"
                 << type << " id;";

              member = "id";
            }
            else
            {
              type = mi.fq_type ();
              db_type_id = member_database_type_id_.database_type_id (mi.m);
            }

            traits = "sqlite::value_traits<\n    "
              + type + ",\n    "
              + db_type_id + " >";
          }

          return true;
        }

        virtual void
        post (member_info& mi)
        {
          if (composite (mi.t))
            return;

          // When handling a pointer, mi.t is the id type of the referenced
          // object.
          //
          semantics::type& mt (member_utype (mi.m, key_prefix_));

          if (object_pointer (mt))
          {
            if (!member_override_.empty ())
              member = member_override_;
            else
            {
              member = "o." + mi.m.name ();

              if (mi.cq)
                member = "const_cast< " + mi.fq_type (false) + "& > (" +
                  member + ")";
            }

            if (lazy_pointer (mt))
              os << member << " = ptr_traits::pointer_type (db, id);";
            else
              os << "// If a compiler error points to the line below, then" << endl
                 << "// it most likely means that a pointer used in a member" << endl
                 << "// cannot be initialized from an object pointer." << endl
                 << "//" << endl
                 << member << " = ptr_traits::pointer_type (" << endl
                 << "db.load< obj_traits::object_type > (id));";

            os << "}"
               << "}";
          }
        }

        virtual void
        traverse_composite (member_info& mi)
        {
          os << traits << "::init (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "db);"
             << endl;
        }

        virtual void
        traverse_integer (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_real (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

        virtual void
        traverse_string (member_info& mi)
        {
          os << traits << "::set_value (" << endl
             << member << "," << endl
             << "i." << mi.var << "value," << endl
             << "i." << mi.var << "size," << endl
             << "i." << mi.var << "null);"
             << endl;
        }

      private:
        string type;
        string db_type_id;
        string traits;
        string member;

        member_database_type_id member_database_type_id_;
      };
      entry<init_value_member> init_value_member_;

      struct container_traits: relational::container_traits, context
      {
        container_traits (base const& x): base (x) {}

        virtual void
        cache_result (string const&)
        {
          // Caching is not necessary since SQLite can execute several
          // interleaving statements.
          //
        }
      };
      entry<container_traits> container_traits_;


      struct class_: relational::class_, context
      {
        class_ (base const& x): base (x) {}

        virtual void
        init_auto_id (semantics::data_member&, string const& im)
        {
          os << im << "null = true;";
        }
      };
      entry<class_> class_entry_;
    }
  }
}
