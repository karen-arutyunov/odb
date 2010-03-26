// file      : odb/semantics/derived.hxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2010 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_DERIVED_HXX
#define ODB_SEMANTICS_DERIVED_HXX

#include <semantics/elements.hxx>

namespace semantics
{
  //
  // Derived types (cvr-qualifiers, pointer, reference, and array).
  //

  class derived_type: public type
  {
  public:
    virtual type&
    base_type () const = 0;
  };

  //
  //
  class qualifier;

  class qualifies: public edge
  {
  public:
    typedef semantics::type type_type;
    typedef semantics::qualifier qualifier_type;

    type_type&
    type () const
    {
      return *type_;
    }

    qualifier_type&
    qualifier () const
    {
      return *qualifier_;
    }

  public:
    qualifies ()
    {
    }

    void
    set_left_node (qualifier_type& n)
    {
      qualifier_ = &n;
    }

    void
    set_right_node (type_type& n)
    {
      type_ = &n;
    }

  protected:
    type_type* type_;
    qualifier_type* qualifier_;
  };

  class qualifier: public derived_type
  {
  public:
    typedef semantics::qualifies qualifies_type;

    bool
    const_ () const
    {
      return c_;
    }

    bool
    volatile_ () const
    {
      return v_;
    }

    bool
    restrict_ () const
    {
      return r_;
    }

    virtual type&
    base_type () const
    {
      return qualifies_->type ();
    }

    qualifies_type&
    qualifies () const
    {
      return *qualifies_;
    }

  public:
    qualifier (path const& file,
               size_t line,
               size_t column,
               bool c,
               bool v,
               bool r)
        : node (file, line, column), c_ (c), v_ (v), r_ (r), qualifies_ (0)
    {
    }

    void
    add_edge_left (qualifies_type& e)
    {
      assert (qualifies_ == 0);
      qualifies_ = &e;
    }

  private:
    bool c_, v_, r_;
    qualifies_type* qualifies_;
  };

  //
  //
  class pointer;

  class points: public edge
  {
  public:
    typedef semantics::type type_type;
    typedef semantics::pointer pointer_type;

    type_type&
    type () const
    {
      return *type_;
    }

    pointer_type&
    pointer () const
    {
      return *pointer_;
    }

  public:
    points ()
    {
    }

    void
    set_left_node (pointer_type& n)
    {
      pointer_ = &n;
    }

    void
    set_right_node (type_type& n)
    {
      type_ = &n;
    }

  protected:
    type_type* type_;
    pointer_type* pointer_;
  };

  class pointer: public type
  {
  public:
    typedef semantics::points points_type;

    virtual type&
    base_type () const
    {
      return points_->type ();
    }

    points_type&
    points () const
    {
      return *points_;
    }

  public:
    pointer (path const& file, size_t line, size_t column)
        : node (file, line, column), points_ (0)
    {
    }

    void
    add_edge_left (points_type& e)
    {
      assert (points_ == 0);
      points_ = &e;
    }

  private:
    points_type* points_;
  };


  //
  //
  class reference;

  class references: public edge
  {
  public:
    typedef semantics::type type_type;
    typedef semantics::reference reference_type;

    type_type&
    type () const
    {
      return *type_;
    }

    reference_type&
    reference () const
    {
      return *reference_;
    }

  public:
    references ()
    {
    }

    void
    set_left_node (reference_type& n)
    {
      reference_ = &n;
    }

    void
    set_right_node (type_type& n)
    {
      type_ = &n;
    }

  protected:
    type_type* type_;
    reference_type* reference_;
  };

  class reference: public type
  {
  public:
    typedef semantics::references references_type;

    virtual type&
    base_type () const
    {
      return references_->type ();
    }

    references_type&
    references () const
    {
      return *references_;
    }

  public:
    reference (path const& file, size_t line, size_t column)
        : node (file, line, column), references_ (0)
    {
    }

    void
    add_edge_left (references_type& e)
    {
      assert (references_ == 0);
      references_ = &e;
    }

  private:
    references_type* references_;
  };


  //
  //
  class array;

  class contains: public edge
  {
  public:
    typedef semantics::type type_type;
    typedef semantics::array array_type;

    type_type&
    type () const
    {
      return *type_;
    }

    array_type&
    array () const
    {
      return *array_;
    }

  public:
    contains ()
    {
    }

    void
    set_left_node (array_type& n)
    {
      array_ = &n;
    }

    void
    set_right_node (type_type& n)
    {
      type_ = &n;
    }

  protected:
    type_type* type_;
    array_type* array_;
  };

  class array: public derived_type
  {
  public:
    typedef semantics::contains contains_type;

    // Return the number of elements in the array or 0 if it is not
    // specified (e.g., int[]).
    //
    unsigned long long
    size () const
    {
      return size_;
    }

    virtual type&
    base_type () const
    {
      return contains_->type ();
    }

    contains_type&
    contains () const
    {
      return *contains_;
    }

  public:
    array (path const& file,
           size_t line,
           size_t column,
           unsigned long long size)
        : node (file, line, column), contains_ (0), size_ (size)
    {
    }

    void
    add_edge_left (contains_type& e)
    {
      assert (contains_ == 0);
      contains_ = &e;
    }

  private:
    contains_type* contains_;
    unsigned long long size_;
  };
}

#endif // ODB_SEMANTICS_DERIVED_HXX
