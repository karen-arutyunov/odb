// file      : odb/sqlite/container-statements.txx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2005-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#include <cstddef> // std::size_t
#include <cstring> // std::memset

namespace odb
{
  namespace sqlite
  {
    // container_statements
    //

    template <typename T>
    container_statements<T>::
    container_statements (connection_type& conn)
        : conn_ (conn),
          functions_ (this,
                      &traits::insert_one,
                      &traits::load_all,
                      &traits::delete_all),
          id_image_ (0),
          cond_image_binding_ (cond_image_bind_, traits::cond_column_count),
          data_image_binding_ (data_image_bind_, traits::data_column_count)
    {
      std::memset (cond_image_bind_, 0, sizeof (cond_image_bind_));
      std::memset (data_image_bind_, 0, sizeof (data_image_bind_));
      std::memset (data_image_truncated_, 0, sizeof (data_image_truncated_));

      for (std::size_t i (0); i < traits::data_column_count; ++i)
        data_image_bind_[i].truncated = data_image_truncated_ + i;
    }
  }
}
