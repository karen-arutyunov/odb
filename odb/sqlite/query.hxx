// file      : odb/sqlite/query.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SQLITE_QUERY_HXX
#define ODB_SQLITE_QUERY_HXX

#include <odb/pre.hxx>

#include <string>
#include <vector>
#include <cstddef> // std::size_t

#include <odb/query.hxx>
#include <odb/details/buffer.hxx>
#include <odb/details/shared-ptr.hxx>

#include <odb/sqlite/version.hxx>
#include <odb/sqlite/forward.hxx>
#include <odb/sqlite/traits.hxx>
#include <odb/sqlite/sqlite-types.hxx>
#include <odb/sqlite/binding.hxx>
#include <odb/sqlite/details/export.hxx>

namespace odb
{
  namespace sqlite
  {
    template <typename T>
    class val_bind
    {
    public:
      explicit
      val_bind (const T& v): val (v) {}

      const T& val;
    };

    template <typename T>
    class ref_bind
    {
    public:
      explicit
      ref_bind (const T& r): ref (r) {}

      const T& ref;
    };

    struct LIBODB_SQLITE_EXPORT query_param: details::shared_base
    {
      virtual
      ~query_param ();

      bool
      reference () const
      {
        return value_ != 0;
      }

      virtual bool
      init () = 0;

      virtual void
      bind (sqlite::bind*) = 0;

    protected:
      query_param (const void* value)
          : value_ (value)
      {
      }

    protected:
      const void* value_;
    };

    class query;

    class LIBODB_SQLITE_EXPORT query_params: public details::shared_base
    {
    public:
      typedef sqlite::binding binding_type;

      binding_type&
      binding ();

    private:
      friend class query;

      query_params (): binding_ (0, 0) {}
      query_params (const query_params&);

      query_params&
      operator= (const query_params&);

      query_params&
      operator+= (const query_params&);

      void
      add (details::shared_ptr<query_param>);

    private:
      typedef std::vector<details::shared_ptr<query_param> > params;

      params params_;
      std::vector<sqlite::bind> bind_;
      binding_type binding_;
    };

    //
    //
    template <typename T, database_type_id ID>
    struct query_column;

    class LIBODB_SQLITE_EXPORT query
    {
    public:
      query ()
        : parameters_ (new (details::shared) query_params)
      {
      }

      explicit
      query (const std::string& q)
        : clause_ (q), parameters_ (new (details::shared) query_params)
      {
      }

      template <typename T>
      explicit
      query (val_bind<T> v)
        : parameters_ (new (details::shared) query_params)
      {
        append<T, type_traits<T>::db_type_id> (v);
      }

      template <typename T>
      explicit
      query (ref_bind<T> r)
        : parameters_ (new (details::shared) query_params)
      {
        append<T, type_traits<T>::db_type_id> (r);
      }

      template <database_type_id ID>
      query (const query_column<bool, ID>&);

      query (const query&);

      query&
      operator= (const query&);

    public:
      std::string
      clause () const;

      binding&
      parameters_binding () const;

      details::shared_ptr<query_params>
      parameters () const;

    public:
      template <typename T>
      static val_bind<T>
      _val (const T& x)
      {
        return val_bind<T> (x);
      }

      template <typename T>
      static ref_bind<T>
      _ref (const T& x)
      {
        return ref_bind<T> (x);
      }

    public:
      query&
      operator+= (const query&);

      query&
      operator+= (const std::string& q)
      {
        size_t n (clause_.size ());

        if (n != 0 && clause_[n - 1] != ' ' && !q.empty () && q[0] != ' ')
          clause_ += ' ';

        clause_ += q;
        return *this;
      }

      template <typename T>
      query&
      operator+= (val_bind<T> v)
      {
        append<T, type_traits<T>::db_type_id> (v);
        return *this;
      }

      template <typename T>
      query&
      operator+= (ref_bind<T> r)
      {
        append<T, type_traits<T>::db_type_id> (r);
        return *this;
      }

    public:
      template <typename T, database_type_id ID>
      void
      append (val_bind<T>);

      template <typename T, database_type_id ID>
      void
      append (ref_bind<T>);

    private:
      void
      add (details::shared_ptr<query_param>);

    private:
      std::string clause_;
      details::shared_ptr<query_params> parameters_;
    };

    inline query
    operator+ (const query& x, const query& y)
    {
      query r (x);
      r += y;
      return r;
    }

    template <typename T>
    inline query
    operator+ (const query& q, val_bind<T> b)
    {
      query r (q);
      r += b;
      return r;
    }

    template <typename T>
    inline query
    operator+ (const query& q, ref_bind<T> b)
    {
      query r (q);
      r += b;
      return r;
    }

    template <typename T>
    inline query
    operator+ (val_bind<T> b, const query& q)
    {
      query r;
      r += b;
      r += q;
      return r;
    }

    template <typename T>
    inline query
    operator+ (ref_bind<T> b, const query& q)
    {
      query r;
      r += b;
      r += q;
      return r;
    }

    inline query
    operator+ (const query& q, const std::string& s)
    {
      query r (q);
      r += s;
      return r;
    }

    inline query
    operator+ (const std::string& s, const query& q)
    {
      query r (s);
      r += q;
      return r;
    }

    template <typename T>
    inline query
    operator+ (const std::string& s, val_bind<T> b)
    {
      query r (s);
      r += b;
      return r;
    }

    template <typename T>
    inline query
    operator+ (const std::string& s, ref_bind<T> b)
    {
      query r (s);
      r += b;
      return r;
    }

    template <typename T>
    inline query
    operator+ (val_bind<T> b, const std::string& s)
    {
      query r;
      r += b;
      r += s;
      return r;
    }

    template <typename T>
    inline query
    operator+ (ref_bind<T> b, const std::string& s)
    {
      query r;
      r += b;
      r += s;
      return r;
    }

    inline query
    operator&& (const query& x, const query& y)
    {
      query r ("(");
      r += x;
      r += ") AND (";
      r += y;
      r += ")";
      return r;
    }

    inline query
    operator|| (const query& x, const query& y)
    {
      query r ("(");
      r += x;
      r += ") OR (";
      r += y;
      r += ")";
      return r;
    }

    inline query
    operator! (const query& x)
    {
      query r ("NOT (");
      r += x;
      r += ")";
      return r;
    }

    // query_column
    //

    template <typename T, typename T2>
    class copy_bind: public val_bind<T>
    {
    public:
      explicit
      copy_bind (const T2& v): val_bind<T> (val), val (v) {}

      const T val;
    };

    template <typename T>
    const T&
    type_instance ();

    template <typename T, database_type_id ID>
    struct query_column
    {
      explicit
      query_column (const char* name)
          : name_ (name)
      {
      }

      const char*
      name () const
      {
        return name_;
      }

      // is_null, is_not_null
      //
    public:
      query
      is_null () const
      {
        query q (name_);
        q += "IS NULL";
        return q;
      }

      query
      is_not_null () const
      {
        query q (name_);
        q += "IS NOT NULL";
        return q;
      }

      // in
      //
    public:
      query
      in (const T&, const T&) const;

      query
      in (const T&, const T&, const T&) const;

      query
      in (const T&, const T&, const T&, const T&) const;

      query
      in (const T&, const T&, const T&, const T&, const T&) const;

      template <typename I>
      query
      in_range (I begin, I end) const;

      // =
      //
    public:
      query
      equal (const T& v) const
      {
        return equal (val_bind<T> (v));
      }

      query
      equal (val_bind<T> v) const
      {
        query q (name_);
        q += "=";
        q.append<T, ID> (v);
        return q;
      }

      template <typename T2>
      query
      equal (val_bind<T2> v) const
      {
        copy_bind<T, T2> c (v.val);
        return equal (c);
      }

      query
      equal (ref_bind<T> r) const
      {
        query q (name_);
        q += "=";
        q.append<T, ID> (r);
        return q;
      }

      friend query
      operator== (const query_column& c, const T& v)
      {
        return c.equal (v);
      }

      friend query
      operator== (const T& v, const query_column& c)
      {
        return c.equal (v);
      }

      friend query
      operator== (const query_column& c, val_bind<T> v)
      {
        return c.equal (v);
      }

      friend query
      operator== (val_bind<T> v, const query_column& c)
      {
        return c.equal (v);
      }

      template <typename T2>
      friend query
      operator== (const query_column& c, val_bind<T2> v)
      {
        return c.equal (v);
      }

      template <typename T2>
      friend query
      operator== (val_bind<T2> v, const query_column& c)
      {
        return c.equal (v);
      }

      friend query
      operator== (const query_column& c, ref_bind<T> r)
      {
        return c.equal (r);
      }

      friend query
      operator== (ref_bind<T> r, const query_column& c)
      {
        return c.equal (r);
      }

      // !=
      //
    public:
      query
      unequal (const T& v) const
      {
        return unequal (val_bind<T> (v));
      }

      query
      unequal (val_bind<T> v) const
      {
        query q (name_);
        q += "!=";
        q.append<T, ID> (v);
        return q;
      }

      template <typename T2>
      query
      unequal (val_bind<T2> v) const
      {
        copy_bind<T, T2> c (v.val);
        return unequal (c);
      }

      query
      unequal (ref_bind<T> r) const
      {
        query q (name_);
        q += "!=";
        q.append<T, ID> (r);
        return q;
      }

      friend query
      operator!= (const query_column& c, const T& v)
      {
        return c.unequal (v);
      }

      friend query
      operator!= (const T& v, const query_column& c)
      {
        return c.unequal (v);
      }

      friend query
      operator!= (const query_column& c, val_bind<T> v)
      {
        return c.unequal (v);
      }

      friend query
      operator!= (val_bind<T> v, const query_column& c)
      {
        return c.unequal (v);
      }

      template <typename T2>
      friend query
      operator!= (const query_column& c, val_bind<T2> v)
      {
        return c.unequal (v);
      }

      template <typename T2>
      friend query
      operator!= (val_bind<T2> v, const query_column& c)
      {
        return c.unequal (v);
      }

      friend query
      operator!= (const query_column& c, ref_bind<T> r)
      {
        return c.unequal (r);
      }

      friend query
      operator!= (ref_bind<T> r, const query_column& c)
      {
        return c.unequal (r);
      }

      // <
      //
    public:
      query
      less (const T& v) const
      {
        return less (val_bind<T> (v));
      }

      query
      less (val_bind<T> v) const
      {
        query q (name_);
        q += "<";
        q.append<T, ID> (v);
        return q;
      }

      template <typename T2>
      query
      less (val_bind<T2> v) const
      {
        copy_bind<T, T2> c (v.val);
        return less (c);
      }

      query
      less (ref_bind<T> r) const
      {
        query q (name_);
        q += "<";
        q.append<T, ID> (r);
        return q;
      }

      friend query
      operator< (const query_column& c, const T& v)
      {
        return c.less (v);
      }

      friend query
      operator< (const T& v, const query_column& c)
      {
        return c.greater (v);
      }

      friend query
      operator< (const query_column& c, val_bind<T> v)
      {
        return c.less (v);
      }

      friend query
      operator< (val_bind<T> v, const query_column& c)
      {
        return c.greater (v);
      }

      template <typename T2>
      friend query
      operator< (const query_column& c, val_bind<T2> v)
      {
        return c.less (v);
      }

      template <typename T2>
      friend query
      operator< (val_bind<T2> v, const query_column& c)
      {
        return c.greater (v);
      }

      friend query
      operator< (const query_column& c, ref_bind<T> r)
      {
        return c.less (r);
      }

      friend query
      operator< (ref_bind<T> r, const query_column& c)
      {
        return c.greater (r);
      }

      // >
      //
    public:
      query
      greater (const T& v) const
      {
        return greater (val_bind<T> (v));
      }

      query
      greater (val_bind<T> v) const
      {
        query q (name_);
        q += ">";
        q.append<T, ID> (v);
        return q;
      }

      template <typename T2>
      query
      greater (val_bind<T2> v) const
      {
        copy_bind<T, T2> c (v.val);
        return greater (c);
      }

      query
      greater (ref_bind<T> r) const
      {
        query q (name_);
        q += ">";
        q.append<T, ID> (r);
        return q;
      }

      friend query
      operator> (const query_column& c, const T& v)
      {
        return c.greater (v);
      }

      friend query
      operator> (const T& v, const query_column& c)
      {
        return c.less (v);
      }

      friend query
      operator> (const query_column& c, val_bind<T> v)
      {
        return c.greater (v);
      }

      friend query
      operator> (val_bind<T> v, const query_column& c)
      {
        return c.less (v);
      }

      template <typename T2>
      friend query
      operator> (const query_column& c, val_bind<T2> v)
      {
        return c.greater (v);
      }

      template <typename T2>
      friend query
      operator> (val_bind<T2> v, const query_column& c)
      {
        return c.less (v);
      }

      friend query
      operator> (const query_column& c, ref_bind<T> r)
      {
        return c.greater (r);
      }

      friend query
      operator> (ref_bind<T> r, const query_column& c)
      {
        return c.less (r);
      }

      // <=
      //
    public:
      query
      less_equal (const T& v) const
      {
        return less_equal (val_bind<T> (v));
      }

      query
      less_equal (val_bind<T> v) const
      {
        query q (name_);
        q += "<=";
        q.append<T, ID> (v);
        return q;
      }

      template <typename T2>
      query
      less_equal (val_bind<T2> v) const
      {
        copy_bind<T, T2> c (v.val);
        return less_equal (c);
      }

      query
      less_equal (ref_bind<T> r) const
      {
        query q (name_);
        q += "<=";
        q.append<T, ID> (r);
        return q;
      }

      friend query
      operator<= (const query_column& c, const T& v)
      {
        return c.less_equal (v);
      }

      friend query
      operator<= (const T& v, const query_column& c)
      {
        return c.greater_equal (v);
      }

      friend query
      operator<= (const query_column& c, val_bind<T> v)
      {
        return c.less_equal (v);
      }

      friend query
      operator<= (val_bind<T> v, const query_column& c)
      {
        return c.greater_equal (v);
      }

      template <typename T2>
      friend query
      operator<= (const query_column& c, val_bind<T2> v)
      {
        return c.less_equal (v);
      }

      template <typename T2>
      friend query
      operator<= (val_bind<T2> v, const query_column& c)
      {
        return c.greater_equal (v);
      }

      friend query
      operator<= (const query_column& c, ref_bind<T> r)
      {
        return c.less_equal (r);
      }

      friend query
      operator<= (ref_bind<T> r, const query_column& c)
      {
        return c.greater_equal (r);
      }

      // >=
      //
    public:
      query
      greater_equal (const T& v) const
      {
        return greater_equal (val_bind<T> (v));
      }

      query
      greater_equal (val_bind<T> v) const
      {
        query q (name_);
        q += ">=";
        q.append<T, ID> (v);
        return q;
      }

      template <typename T2>
      query
      greater_equal (val_bind<T2> v) const
      {
        copy_bind<T, T2> c (v.val);
        return greater_equal (c);
      }

      query
      greater_equal (ref_bind<T> r) const
      {
        query q (name_);
        q += ">=";
        q.append<T, ID> (r);
        return q;
      }

      friend query
      operator>= (const query_column& c, const T& v)
      {
        return c.greater_equal (v);
      }

      friend query
      operator>= (const T& v, const query_column& c)
      {
        return c.less_equal (v);
      }

      friend query
      operator>= (const query_column& c, val_bind<T> v)
      {
        return c.greater_equal (v);
      }

      friend query
      operator>= (val_bind<T> v, const query_column& c)
      {
        return c.less_equal (v);
      }

      template <typename T2>
      friend query
      operator>= (const query_column& c, val_bind<T2> v)
      {
        return c.greater_equal (v);
      }

      template <typename T2>
      friend query
      operator>= (val_bind<T2> v, const query_column& c)
      {
        return c.less_equal (v);
      }

      friend query
      operator>= (const query_column& c, ref_bind<T> r)
      {
        return c.greater_equal (r);
      }

      friend query
      operator>= (ref_bind<T> r, const query_column& c)
      {
        return c.less_equal (r);
      }

      // Column comparison.
      //
    public:
      template <typename T2, database_type_id ID2>
      query
      operator== (const query_column<T2, ID2>& c) const
      {
        // We can compare columns only if we can compare their C++ types.
        //
        (void) (sizeof (type_instance<T> () == type_instance<T2> ()));

        query q (name_);
        q += "=";
        q += c.name ();
        return q;
      }

      template <typename T2, database_type_id ID2>
      query
      operator!= (const query_column<T2, ID2>& c) const
      {
        // We can compare columns only if we can compare their C++ types.
        //
        (void) (sizeof (type_instance<T> () != type_instance<T2> ()));

        query q (name_);
        q += "!=";
        q += c.name ();
        return q;
      }

      template <typename T2, database_type_id ID2>
      query
      operator< (const query_column<T2, ID2>& c) const
      {
        // We can compare columns only if we can compare their C++ types.
        //
        (void) (sizeof (type_instance<T> () < type_instance<T2> ()));

        query q (name_);
        q += "<";
        q += c.name ();
        return q;
      }

      template <typename T2, database_type_id ID2>
      query
      operator> (const query_column<T2, ID2>& c) const
      {
        // We can compare columns only if we can compare their C++ types.
        //
        (void) (sizeof (type_instance<T> () > type_instance<T2> ()));

        query q (name_);
        q += ">";
        q += c.name ();
        return q;
      }

      template <typename T2, database_type_id ID2>
      query
      operator<= (const query_column<T2, ID2>& c) const
      {
        // We can compare columns only if we can compare their C++ types.
        //
        (void) (sizeof (type_instance<T> () <= type_instance<T2> ()));

        query q (name_);
        q += "<=";
        q += c.name ();
        return q;
      }

      template <typename T2, database_type_id ID2>
      query
      operator>= (const query_column<T2, ID2>& c) const
      {
        // We can compare columns only if we can compare their C++ types.
        //
        (void) (sizeof (type_instance<T> () >= type_instance<T2> ()));

        query q (name_);
        q += ">=";
        q += c.name ();
        return q;
      }

    private:
      const char* name_;
    };

    //
    //
    template <typename T, database_type_id>
    struct query_param_impl;

    // INTEGER
    //
    template <typename T>
    struct query_param_impl<T, id_integer>: query_param
    {
      query_param_impl (ref_bind<T> r) : query_param (&r.ref) {}
      query_param_impl (val_bind<T> v) : query_param (0) {init (v.val);}

      virtual bool
      init ()
      {
        init (*static_cast<const T*> (value_));
        return false;
      }

      virtual void
      bind (sqlite::bind* b)
      {
        b->type = sqlite::bind::integer;
        b->buffer = &image_;
      }

    private:
      void
      init (const T& v)
      {
        bool dummy;
        value_traits<T, long long, id_integer>::set_image (image_, dummy, v);
      }

    private:
      long long image_;
    };

    // REAL
    //
    template <typename T>
    struct query_param_impl<T, id_real>: query_param
    {
      query_param_impl (ref_bind<T> r) : query_param (&r.ref) {}
      query_param_impl (val_bind<T> v) : query_param (0) {init (v.val);}

      virtual bool
      init ()
      {
        init (*static_cast<const T*> (value_));
        return false;
      }

      virtual void
      bind (sqlite::bind* b)
      {
        b->type = sqlite::bind::real;
        b->buffer = &image_;
      }

    private:
      void
      init (const T& v)
      {
        bool dummy;
        value_traits<T, double, id_real>::set_image (image_, dummy, v);
      }

    private:
      double image_;
    };

    // TEXT
    //
    template <typename T>
    struct query_param_impl<T, id_text>: query_param
    {
      query_param_impl (ref_bind<T> r) : query_param (&r.ref) {}
      query_param_impl (val_bind<T> v) : query_param (0) {init (v.val);}

      virtual bool
      init ()
      {
        return init (*static_cast<const T*> (value_));
      }

      virtual void
      bind (sqlite::bind* b)
      {
        b->type = sqlite::bind::text;
        b->buffer = buffer_.data ();
        b->size = &size_;
      }

    private:
      bool
      init (const T& v)
      {
        bool dummy;
        std::size_t cap (buffer_.capacity ());
        value_traits<T, details::buffer, id_text>::set_image (
          buffer_, size_, dummy, v);
        return cap != buffer_.capacity ();
      }

    private:
      details::buffer buffer_;
      std::size_t size_;
    };

    // BLOB
    //
    template <typename T>
    struct query_param_impl<T, id_blob>: query_param
    {
      query_param_impl (ref_bind<T> r) : query_param (&r.ref) {}
      query_param_impl (val_bind<T> v) : query_param (0) {init (v.val);}

      virtual bool
      init ()
      {
        return init (*static_cast<const T*> (value_));
      }

      virtual void
      bind (sqlite::bind* b)
      {
        b->type = sqlite::bind::blob;
        b->buffer = buffer_.data ();
        b->size = &size_;
      }

    private:
      bool
      init (const T& v)
      {
        bool dummy;
        std::size_t cap (buffer_.capacity ());
        value_traits<T, details::buffer, id_blob>::set_image (
          buffer_, size_, dummy, v);
        return cap != buffer_.capacity ();
      }

    private:
      details::buffer buffer_;
      std::size_t size_;
    };
  }
}

// odb::query specialization for SQLite.
//
namespace odb
{
  template <typename T>
  class query<T, sqlite::query>: public object_traits<T>::query_type
  {
  public:
    // We don't define any typedefs here since they may clash with
    // column names defined by our base type.
    //

    query ()
    {
    }

    explicit
    query (const std::string& q)
        : object_traits<T>::query_type (q)
    {
    }

    template <typename T2>
    explicit
    query (sqlite::val_bind<T2> v)
        : object_traits<T>::query_type (sqlite::query (v))
    {
    }

    template <typename T2>
    explicit
    query (sqlite::ref_bind<T2> r)
        : object_traits<T>::query_type (sqlite::query (r))
    {
    }

    query (const sqlite::query& q)
        : object_traits<T>::query_type (q)
    {
    }

    template <sqlite::database_type_id ID>
    query (const sqlite::query_column<bool, ID>& qc)
        : object_traits<T>::query_type (qc)
    {
    }
  };
}

#include <odb/sqlite/query.ixx>
#include <odb/sqlite/query.txx>

#include <odb/post.hxx>

#endif // ODB_SQLITE_QUERY_HXX
