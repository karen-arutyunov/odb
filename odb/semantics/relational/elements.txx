// file      : odb/semantics/relational/elements.txx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

namespace semantics
{
  namespace relational
  {
    // scope
    //

    template <typename N>
    typename scope<N>::names_iterator scope<N>::
    find (name_type const& name)
    {
      typename names_map::iterator i (names_map_.find (name));

      if (i == names_map_.end ())
        return names_.end ();
      else
        return i->second;
    }

    template <typename N>
    typename scope<N>::names_const_iterator scope<N>::
    find (name_type const& name) const
    {
      typename names_map::const_iterator i (names_map_.find (name));

      if (i == names_map_.end ())
        return names_.end ();
      else
        return names_const_iterator (i->second);
    }

    template <typename N>
    typename scope<N>::names_iterator scope<N>::
    find (names_type const& e)
    {
      typename names_iterator_map::iterator i (iterator_map_.find (&e));
      return i != iterator_map_.end () ? i->second : names_.end ();
    }

    template <typename N>
    typename scope<N>::names_const_iterator scope<N>::
    find (names_type const& e) const
    {
      typename names_iterator_map::const_iterator i (iterator_map_.find (&e));
      return i != iterator_map_.end () ? i->second : names_.end ();
    }

    class column;
    class primary_key;

    template <typename N>
    void scope<N>::
    add_edge_left (names_type& e)
    {
      nameable_type& n (e.nameable ());
      name_type const& name (e.name ());

      typename names_map::iterator i (names_map_.find (name));

      if (i == names_map_.end ())
      {
        typename names_list::iterator i;

        // We want the order to be columns first, then the primary key,
        // and then the foreign keys.
        //
        if (n.is_a<column> ())
          i = names_.insert (first_key_, &e);
        else
        {
          if (n.is_a<primary_key> ())
            first_key_ = i = names_.insert (first_key_, &e);
          else
          {
            i = names_.insert (names_.end (), &e);

            if (first_key_ == names_.end ())
              first_key_ = i;
          }
        }

        names_map_[name] = i;
        iterator_map_[&e] = i;
      }
      else
        throw duplicate_name (*this, (*i->second)->nameable (), n);
    }
  }
}
