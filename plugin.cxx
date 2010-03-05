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

enum class_access { ca_public, ca_protected, ca_private };
const char* class_access_str[] = {"public", "protected", "private"};

class collector
{
public:
  collector ()
      : file_ (main_input_filename)
  {
  }

  void
  traverse (tree global_scope)
  {
    traverse_namespace (global_scope);
  }

private:
  void
  traverse_namespace (tree ns)
  {
    cp_binding_level* level = NAMESPACE_LEVEL (ns);
    tree decl = level->names;

    for (; decl != NULL_TREE; decl = TREE_CHAIN (decl))
    {
      switch (TREE_CODE (decl))
      {
        /*
          case FUNCTION_DECL:
          {
          if (DECL_ANTICIPATED (decl))
          break;

          warning (0, G_ ("function declaration %s"),
          IDENTIFIER_POINTER (DECL_NAME (decl)));
          break;
          }
        */

        /*
          case RECORD_TYPE:
          {
          warning (0, G_ ("class declaration %s"),
          IDENTIFIER_POINTER (DECL_NAME (decl)));
          break;
          }
        */

      case TYPE_DECL:
        {
          if (DECL_ARTIFICIAL (decl))
          {
            tree type = TREE_TYPE (decl);
            tree name = DECL_NAME (decl);

            if (name != NULL_TREE && TREE_CODE (type) == RECORD_TYPE)
            {
              warning (0, G_ ("class declaration %s in %s:%i"),
                       IDENTIFIER_POINTER (name),
                       DECL_SOURCE_FILE (decl),
                       DECL_SOURCE_LINE (decl));

              traverse_class (type);
            }
          }

          break;
        }
      case NAMESPACE_DECL:
        {
          tree target = DECL_NAMESPACE_ALIAS (decl);
          assert (target != NULL_TREE);

          warning (0, G_ ("namespace alias declaration %s=%s in %s:%i"),
                   IDENTIFIER_POINTER (DECL_NAME (decl)),
                   IDENTIFIER_POINTER (DECL_NAME (target)),
                   DECL_SOURCE_FILE (decl),
                   DECL_SOURCE_LINE (decl));
          break;
        }
      default:
        {
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
          break;
        }
      }

      //warning (0, G_ ("declaration %u"), (int) (TREE_CODE (decl)));


      //warning (0, G_ ("declaration %s"),
      //         IDENTIFIER_POINTER (DECL_NAME (decl)));
    }

    for(decl = level->namespaces; decl != NULL_TREE; decl = TREE_CHAIN (decl))
    {
      if (DECL_NAMESPACE_STD_P (decl) || DECL_IS_BUILTIN (decl))
        continue;

      tree name = DECL_NAME (decl);

      if (name == NULL_TREE)
      {
        warning (0, G_ ("anonymous namespace declaration in %s:%i"),
                 DECL_SOURCE_FILE (decl),
                 DECL_SOURCE_LINE (decl));
      }
      else
      {
        warning (0, G_ ("namespace declaration %s in %s:%i"),
                 IDENTIFIER_POINTER (name),
                 DECL_SOURCE_FILE (decl),
                 DECL_SOURCE_LINE (decl));
      }

      traverse_namespace (decl);
    }
  }

  void
  traverse_class (tree c)
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
          //if (!DECL_IS_BUILTIN (decl))
          //{
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
          //}
          break;
        }
      }
    }
  }

private:
  string file_;
};

extern "C" void
start_unit_callback (void* gcc_data, void* user_data)
{
  warning (0, G_ ("strating unit processing"));
}

extern "C" void
finish_type_callback (void* gcc_data, void* user_data)
{
  warning (0, G_ ("finished type processing"));
}

extern "C" void
finish_unit_callback (void* gcc_data, void* user_data)
{
  //traverse_namespace (global_namespace);
  //done = true;
  warning (0, G_ ("finished unit processing"));
}

extern "C" void
gate_callback (void* gcc_data, void* user_data)
{
  warning (0, G_ ("main file is %s"), main_input_filename);

  if (!errorcount && !sorrycount)
  {
    collector c;
    c.traverse (global_namespace);
  }

  exit (0);

  // Disable every pass.
  //
  /*
  warning (0, G_ ("disabling pass %s"), current_pass->name);
  *static_cast<int*> (gcc_data) = 0;
  */
}

extern "C" int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  warning (0, G_ ("strating plugin %s"), plugin_info->base_name);

  // Disable assembly output.
  //
  asm_file_name = HOST_BIT_BUCKET;

  //register_callback (plugin_info->base_name, PLUGIN_START_UNIT, &start_unit_callback, NULL);
  //register_callback (plugin_info->base_name, PLUGIN_FINISH_TYPE, &finish_type_callback, NULL);
  //register_callback (plugin_info->base_name, PLUGIN_FINISH_UNIT, &finish_unit_callback, NULL);
  register_callback (plugin_info->base_name, PLUGIN_OVERRIDE_GATE, &gate_callback, NULL);

  return 0;
}
