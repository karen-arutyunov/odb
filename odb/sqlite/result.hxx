// file      : odb/sqlite/result.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_RESULT_HXX
#define ODB_SQLITE_RESULT_HXX

#include <odb/pre.hxx>

#include <cstddef> // std::size_t

#include <odb/result.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/query.hxx>
#include <odb/sqlite/statement.hxx>

#include <odb/details/shared-ptr.hxx>

namespace odb
{
  namespace sqlite
  {
    class result_impl_base
    {
    public:
      result_impl_base (const query&, details::shared_ptr<select_statement>);

    protected:
      // We need to hold on to the query parameters because SQLite uses
      // the parameter buffers to find each next row.
      //
      details::shared_ptr<query_params> params_;
      details::shared_ptr<select_statement> statement_;
    };

    template <typename T>
    class result_impl: public odb::result_impl<T>, public result_impl_base
    {
    public:
      typedef typename odb::result_impl<T>::pointer_type pointer_type;
      typedef typename odb::result_impl<T>::pointer_traits pointer_traits;

      typedef typename odb::result_impl<T>::object_type object_type;
      typedef typename odb::result_impl<T>::id_type id_type;
      typedef typename odb::result_impl<T>::object_traits object_traits;


      virtual
      ~result_impl ();

      result_impl (const query&,
                   details::shared_ptr<select_statement>,
                   object_statements<object_type>&);

      virtual void
      load (object_type&);

      virtual id_type
      load_id ();

      virtual void
      next ();

      virtual void
      cache ();

      virtual std::size_t
      size ();

      using odb::result_impl<T>::current;

    private:
      void
      load_image ();

    private:
      object_statements<object_type>& statements_;
    };
  }
}

#include <odb/sqlite/result.txx>

#include <odb/post.hxx>

#endif // ODB_SQLITE_RESULT_HXX
