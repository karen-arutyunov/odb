// file      : odb/details/tls.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_DETAILS_TLS_HXX
#define ODB_DETAILS_TLS_HXX

#include <odb/pre.hxx>

#include <odb/details/config.hxx>

#ifdef ODB_THREADS_NONE

#  define ODB_TLS_POINTER(type) type*
#  define ODB_TLS_OBJECT(type) type

namespace odb
{
  namespace details
  {
    template <typename T>
    inline T&
    tls_get (T& x)
    {
      return x;
    }

    template <typename T>
    inline T*
    tls_get (T* p)
    {
      return p;
    }

    template <typename T>
    inline void
    tls_set (T*& rp, T* p)
    {
      rp = p;
    }
  }
}

#elif defined(ODB_THREADS_POSIX)

#  include <odb/details/posix/tls.hxx>
#  define ODB_TLS_POINTER(type) tls<type*>
#  define ODB_TLS_OBJECT(type) tls<type>

#elif defined(ODB_THREADS_WIN32)

#  ifdef ODB_THREADS_TLS_DECLSPEC_POINTER
#    define ODB_TLS_POINTER(type) __declspec(thread) type*

namespace odb
{
  namespace details
  {
    template <typename T>
    inline T*
    tls_get (T* p)
    {
      return p;
    }

    template <typename T>
    inline void
    tls_set (T*& rp, T* p)
    {
      rp = p;
    }
  }
}

#  else
#    error unsupported TLS pointer model
#  endif

#  ifdef ODB_THREADS_TLS_DECLSPEC_OBJECT
#    define ODB_TLS_OBJECT(type) __declspec(thread) type

namespace odb
{
  namespace details
  {
    template <typename T>
    inline T&
    tls_get (T& x)
    {
      return x;
    }
  }
}

#  else
#    error unsupported TLS object model
#  endif

#else
# error unknown threading model
#endif

#include <odb/post.hxx>

#endif // ODB_DETAILS_TLS_HXX