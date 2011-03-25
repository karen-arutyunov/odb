// file      : odb/sqlite/result.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <odb/exceptions.hxx>

namespace odb
{
  namespace sqlite
  {
    template <typename T>
    result_impl<T>::
    ~result_impl ()
    {
    }

    template <typename T>
    result_impl<T>::
    result_impl (const query& q,
                 details::shared_ptr<select_statement> st,
                 object_statements<object_type>& sts)
        : odb::result_impl<T> (sts.connection ().database ()),
          result_impl_base (q, st),
          statements_ (sts)
    {
    }

    template <typename T>
    void result_impl<T>::
    load (object_type& obj)
    {
      load_image ();

      // This is a top-level call so the statements cannot be locked.
      //
      assert (!statements_.locked ());
      typename object_statements<object_type>::auto_lock l (statements_);

      typename object_traits::image_type& im (statements_.image ());
      object_traits::init (obj, im, this->database ());

      // Initialize the id image and load the rest of the object
      // (containers, etc).
      //
      object_traits::init (statements_.id_image (), object_traits::id (im));
      object_traits::load_ (statements_, obj);

      statements_.load_delayed ();
      l.unlock ();
    }

    template <typename T>
    typename result_impl<T>::id_type result_impl<T>::
    load_id ()
    {
      load_image ();
      return object_traits::id (statements_.image ());
    }

    template <typename T>
    void result_impl<T>::
    next ()
    {
      this->current (pointer_type ());

      if (!statement_->next ())
        this->end_ = true;
    }

    template <typename T>
    void result_impl<T>::
    load_image ()
    {
      // The image can grow between calls to load() as a result of other
      // statements execution.
      //
      typename object_traits::image_type& im (statements_.image ());

      if (im.version != statements_.out_image_version ())
      {
        binding& b (statements_.out_image_binding ());
        object_traits::bind (b.bind, im, true);
        statements_.out_image_version (im.version);
        b.version++;
      }

      select_statement::result r (statement_->load ());

      if (r == select_statement::truncated)
      {
        object_traits::grow (im, statements_.out_image_truncated ());

        if (im.version != statements_.out_image_version ())
        {
          binding& b (statements_.out_image_binding ());
          object_traits::bind (b.bind, im, true);
          statements_.out_image_version (im.version);
          b.version++;
          statement_->reload ();
        }
      }
    }

    template <typename T>
    void result_impl<T>::
    cache ()
    {
    }

    template <typename T>
    std::size_t result_impl<T>::
    size ()
    {
      // @@ Should we rather throw unsupported_operation or some such?
      //
      throw result_not_cached ();
    }
  }
}
