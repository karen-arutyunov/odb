// file      : odb/details/posix/tls.ixx
// license   : GNU GPL v2; see accompanying LICENSE file

namespace odb
{
  namespace details
  {
    template <typename T>
    inline tls<T>::
    tls ()
    {
    }

    template <typename T>
    inline tls<T*>::
    tls ()
    {
    }
  }
}