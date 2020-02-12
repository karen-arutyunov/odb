// file      : odb/polymorphic-map.ixx
// license   : GNU GPL v2; see accompanying LICENSE file

namespace odb
{
  template <typename T, database_id DB>
  inline polymorphic_entry<T, DB>::
  polymorphic_entry ()
  {
    polymorphic_entry_impl<root_type, DB>::insert (object_traits::info);
  }

  template <typename T, database_id DB>
  inline polymorphic_entry<T, DB>::
  ~polymorphic_entry ()
  {
    polymorphic_entry_impl<root_type, DB>::erase (object_traits::info);
  }
}
