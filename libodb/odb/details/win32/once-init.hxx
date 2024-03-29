// file      : odb/details/win32/once-init.hxx
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_DETAILS_WIN32_ONCE_INIT_HXX
#define ODB_DETAILS_WIN32_ONCE_INIT_HXX

#include <odb/pre.hxx>

namespace odb
{
  namespace details
  {
    void
    once_process_start ();

    void
    once_process_end (bool safe);
  }
}

#include <odb/post.hxx>

#endif // ODB_DETAILS_WIN32_ONCE_INIT_HXX
