// file      : odb/sqlite/view-statements.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_VIEW_STATEMENTS_HXX
#define ODB_SQLITE_VIEW_STATEMENTS_HXX

#include <odb/pre.hxx>

#include <cstddef> // std::size_t

#include <odb/forward.hxx>
#include <odb/traits.hxx>

#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/binding.hxx>
#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/statements-base.hxx>

namespace odb
{
  namespace sqlite
  {
    template <typename T>
    class view_statements: public statements_base
    {
    public:
      typedef T view_type;
      typedef odb::view_traits<view_type> view_traits;
      typedef typename view_traits::pointer_type pointer_type;
      typedef typename view_traits::image_type image_type;

      typedef sqlite::select_statement query_statement_type;

    public:
      view_statements (connection_type&);

      virtual
      ~view_statements ();

      // View image.
      //
      image_type&
      image ()
      {
        return image_;
      }

      std::size_t
      image_version () const
      {
        return image_version_;
      }

      void
      image_version (std::size_t v)
      {
        image_version_ = v;
      }

      binding&
      image_binding ()
      {
        return image_binding_;
      }

      bool*
      image_truncated ()
      {
        return image_truncated_;
      }

      query_statement_type&
      query_statement ()
      {
        if (query_ == 0)
        {
          query_.reset (
            new (details::shared) query_statement_type (
              conn_,
              view_traits::query_statement,
              image_binding_));

          query_->cached (true);
        }

        return *query_;
      }

    private:
      view_statements (const view_statements&);
      view_statements& operator= (const view_statements&);

    private:
      image_type image_;
      std::size_t image_version_;
      binding image_binding_;
      bind image_bind_[view_traits::column_count];
      bool image_truncated_[view_traits::column_count];

      details::shared_ptr<query_statement_type> query_;
    };
  }
}

#include <odb/sqlite/view-statements.txx>

#include <odb/post.hxx>

#endif // ODB_SQLITE_VIEW_STATEMENTS_HXX
