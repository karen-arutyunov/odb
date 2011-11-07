// file      : odb/sqlite/statement-cache.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_STATEMENT_CACHE_HXX
#define ODB_SQLITE_STATEMENT_CACHE_HXX

#include <odb/pre.hxx>

#include <map>
#include <typeinfo>

#include <odb/forward.hxx>

#include <odb/details/shared-ptr.hxx>
#include <odb/details/type-info.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/statement.hxx>
#include <odb/sqlite/statements-base.hxx>
#include <odb/sqlite/object-statements.hxx>
#include <odb/sqlite/view-statements.hxx>

#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    class connection;

    class LIBODB_SQLITE_EXPORT statement_cache
    {
    public:
      statement_cache (connection&);

      generic_statement&
      begin_statement () const
      {
        return *begin_;
      }

      generic_statement&
      begin_immediate_statement () const
      {
        if (!begin_immediate_)
          begin_immediate_statement_ ();

        return *begin_immediate_;
      }

      generic_statement&
      begin_exclusive_statement () const
      {
        if (!begin_exclusive_)
          begin_exclusive_statement_ ();

        return *begin_exclusive_;
      }

      generic_statement&
      commit_statement () const
      {
        return *commit_;
      }

      generic_statement&
      rollback_statement () const
      {
        return *rollback_;
      }

      template <typename T>
      typename object_statements_selector<T>::type&
      find_object ()
      {
        typedef typename object_statements_selector<T>::type object_statements;

        map::iterator i (map_.find (&typeid (T)));

        if (i != map_.end ())
          return static_cast<object_statements&> (*i->second);

        details::shared_ptr<object_statements> p (
          new (details::shared) object_statements (conn_));

        map_.insert (map::value_type (&typeid (T), p));
        return *p;
      }

      template <typename T>
      view_statements<T>&
      find_view ()
      {
        map::iterator i (map_.find (&typeid (T)));

        if (i != map_.end ())
          return static_cast<view_statements<T>&> (*i->second);

        details::shared_ptr<view_statements<T> > p (
          new (details::shared) view_statements<T> (conn_));

        map_.insert (map::value_type (&typeid (T), p));
        return *p;
      }

    private:
      void
      begin_immediate_statement_ () const;

      void
      begin_exclusive_statement_ () const;

    private:
      typedef std::map<const std::type_info*,
                       details::shared_ptr<statements_base>,
                       details::type_info_comparator> map;

      connection& conn_;

      details::shared_ptr<generic_statement> begin_;
      mutable details::shared_ptr<generic_statement> begin_immediate_;
      mutable details::shared_ptr<generic_statement> begin_exclusive_;
      details::shared_ptr<generic_statement> commit_;
      details::shared_ptr<generic_statement> rollback_;

      map map_;
    };
  }
}

#include <odb/post.hxx>

#endif // ODB_SQLITE_STATEMENT_CACHE_HXX
