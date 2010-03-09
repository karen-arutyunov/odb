extern "C"
{
#include "gcc-plugin.h"
}

#include <stdlib.h>
#include <gmp.h>

extern "C"
{
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "tree-pass.h"
#include "intl.h"

/* reqs */
#include "tm.h"

/* gcc/ headers. */
#include "diagnostic.h"
#include "c-common.h"
#include "c-pretty-print.h"
#include "tree-iterator.h"
#include "plugin.h"
#include "tree-flow.h"
#include "langhooks.h"
#include "cp/cp-tree.h"
#include "cp/cxx-pretty-print.h"
#include "cp/name-lookup.h"
}

#include <map>
#include <string>
#include <cassert>

using namespace std;

int plugin_is_GPL_compatible;

// TODO:
//
//
// * Can plugin define a macro which can then be tested in the code?
//   A wrapper that calls g++ with plugin can do that easily.
//
// * Will need to disable as many warnings as possible.
//
// * How am I going to handle a case where the type of a private
//   member is also private (i.e., local class or typedef -- fairly
//   common).
//

enum class_access { ca_public, ca_protected, ca_private };
const char* class_access_str[] = {"public", "protected", "private"};

class traverser
{
public:
  traverser ()
      : file_ (main_input_filename)
  {
  }

  void
  traverse (tree scope)
  {
    // First collect all the declarations we are interested in
    // in the line-decl map so that they appear in the source
    // code order.
    //
    collect (scope);
    emit ();
  }

private:
  void
  collect (tree ns)
  {
    cp_binding_level* level = NAMESPACE_LEVEL (ns);
    tree decl = level->names;

    // Collect declarations.
    //
    for (; decl != NULL_TREE; decl = TREE_CHAIN (decl))
    {
      switch (TREE_CODE (decl))
      {
      case TYPE_DECL:
        {
          if (DECL_ARTIFICIAL (decl) &&
              DECL_NAME (decl) != NULL_TREE &&
              TREE_CODE (TREE_TYPE (decl)) == RECORD_TYPE &&
              DECL_SOURCE_FILE (decl) == file_)
          {
            decls_[DECL_SOURCE_LINE (decl)] = decl;
          }

          break;
        }
      default:
        {
          /*
          if (!DECL_IS_BUILTIN (decl))
          {
            tree name = DECL_NAME (decl);

            if (name != NULL_TREE)
            {
              warning (0, G_ ("some declaration %s in %s:%i"),
                       IDENTIFIER_POINTER (name),
                       DECL_SOURCE_FILE (decl),
                       DECL_SOURCE_LINE (decl));
            }
            else
            {
              warning (0, G_ ("some unnamed declaration in %s:%i"),
                       DECL_SOURCE_FILE (decl),
                       DECL_SOURCE_LINE (decl));
            }
          }
          */
          break;
        }
      }
    }

    // Traverse namespaces.
    //
    for(decl = level->namespaces; decl != NULL_TREE; decl = TREE_CHAIN (decl))
    {
      if (!DECL_NAMESPACE_STD_P (decl) &&
          !DECL_IS_BUILTIN (decl) &&
          DECL_SOURCE_FILE (decl) == file_)
      {
        tree name = DECL_NAME (decl);

        warning (0, G_ ("namespace declaration %s in %s:%i"),
                 name ? IDENTIFIER_POINTER (name) : "<anonymous>",
                 DECL_SOURCE_FILE (decl),
                 DECL_SOURCE_LINE (decl));

        collect (decl);
      }
    }
  }

  void
  emit ()
  {
    for (decl_map::const_iterator i (decls_.begin ()), e (decls_.end ());
         i != e; ++i)
    {
      tree decl (i->second);

      switch (TREE_CODE (decl))
      {
      case TYPE_DECL:
        {
          tree type = TREE_TYPE (decl);
          tree name = DECL_NAME (decl);

          warning (0, G_ ("class declaration %s in %s:%i"),
                   IDENTIFIER_POINTER (name),
                   DECL_SOURCE_FILE (decl),
                   DECL_SOURCE_LINE (decl));

          emit_class (type);
          break;
        }
      }
    }
  }

  void
  emit_class (tree c)
  {
    // Traverse base information.
    //
    tree bis (TYPE_BINFO (c));
    size_t n (bis ? BINFO_N_BASE_BINFOS (bis) : 0);

    for (size_t i (0); i < n; i++)
    {
      tree bi (BINFO_BASE_BINFO (bis, i));

      class_access a (ca_public);

      if (BINFO_BASE_ACCESSES (bis))
      {
        tree ac (BINFO_BASE_ACCESS (bis, i));

        if (ac == NULL_TREE || ac == access_public_node)
        {
          a = ca_public;
        }
        else if (ac == access_protected_node)
        {
          a = ca_protected;
        }
        else
        {
          assert (ac == access_private_node);
          a = ca_private;
        }
      }

      bool v (BINFO_VIRTUAL_P (bi));
      tree b (BINFO_TYPE (bi));
      tree b_decl (TYPE_NAME (b)); // Typedef decl for this base.

      warning (0, G_ ("\t%s%s base %s"),
               class_access_str[a],
               (v ? " virtual" : ""),
               IDENTIFIER_POINTER (DECL_NAME (b_decl)));
    }

    // Traverse data members.
    //
    for (tree decl (TYPE_FIELDS (c));
         decl != NULL_TREE ;
         decl = TREE_CHAIN (decl))
    {
      //if (DECL_ARTIFICIAL (field))
      //  continue;

      // if (TREE_CODE (field) == TYPE_DECL && TREE_TYPE (field) == c)
      //   continue;

      switch (TREE_CODE (decl))
      {
      case FIELD_DECL:
        {
          if (!DECL_ARTIFICIAL (decl))
          {
            tree name = DECL_NAME (decl);
            tree type = TREE_TYPE (decl);
            tree type_decl = TYPE_NAME (type);

            warning (0, G_ ("\tdata member declaration %s %s in %s:%i"),
                     (type_decl ? IDENTIFIER_POINTER (DECL_NAME (type_decl)) : "?"),
                     IDENTIFIER_POINTER (name),
                     DECL_SOURCE_FILE (decl),
                     DECL_SOURCE_LINE (decl));
            break;
          }
        }
      default:
        {
          /*
          tree name = DECL_NAME (decl);

          if (name != NULL_TREE)
          {
            warning (0, G_ ("\tsome declaration %s in %s:%i"),
                     IDENTIFIER_POINTER (name),
                     DECL_SOURCE_FILE (decl),
                     DECL_SOURCE_LINE (decl));
          }
          else
          {
            warning (0, G_ ("\tsome unnamed declaration in %s:%i"),
                     DECL_SOURCE_FILE (decl),
                     DECL_SOURCE_LINE (decl));
          }
          */
          break;
        }
      }
    }
  }

private:
  typedef map<size_t, tree> decl_map;

  string file_;
  decl_map decls_;
};

extern "C" void
gate_callback (void* gcc_data, void* user_data)
{
  warning (0, G_ ("main file is %s"), main_input_filename);

  if (!errorcount && !sorrycount)
  {
    traverser t;
    t.traverse (global_namespace);
  }

  exit (0);
}

extern "C" int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  warning (0, G_ ("starting plugin %s"), plugin_info->base_name);

  // Disable assembly output.
  //
  asm_file_name = HOST_BIT_BUCKET;

  register_callback (plugin_info->base_name,
                     PLUGIN_OVERRIDE_GATE,
                     &gate_callback,
                     NULL);

  return 0;
}
