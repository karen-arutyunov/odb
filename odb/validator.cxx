// file      : odb/validator.cxx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#include <iostream>

#include <odb/traversal.hxx>
#include <odb/context.hxx>
#include <odb/validator.hxx>

using namespace std;

namespace
{
  struct data_member: traversal::data_member
  {
    data_member (bool& valid)
        : valid_ (valid)
    {
    }

    virtual void
    traverse (type& m)
    {
      if (m.count ("transient"))
        return;

      count_++;
      semantics::type& type (m.type ());
      semantics::belongs& b (m.belongs ());

      if (type.fq_anonymous (b.hint ()))
      {
        cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " error: unnamed type in data member declaration" << endl;

        cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
             << " info: use 'typedef' to name this type" << endl;

        valid_ = false;
      }
    }

    bool& valid_;
    size_t count_;
  };

  // Find id member.
  //
  struct id_member: traversal::class_
  {
    id_member (bool object, bool& valid, semantics::data_member*& m)
        : object_ (object), member_ (valid, m)
    {
      *this >> inherits_ >> *this;
      *this >> names_ >> member_;
    }

    virtual void
    traverse (semantics::class_& c)
    {
      // Skip transient bases.
      //
      if (object_)
      {
        if (!c.count ("object"))
          return;
      }
      else
      {
        if (!context::comp_value (c))
          return;
      }

      inherits (c);
      names (c);
    }

  private:
    struct member: traversal::data_member
    {
      member (bool& valid, semantics::data_member*& m)
          : valid_ (valid), m_ (m)
      {
      }

      virtual void
      traverse (semantics::data_member& m)
      {
        if (m.count ("id"))
        {
          if (m_ != 0)
          {
            cerr << m.file () << ":" << m.line () << ":" << m.column () << ":"
                 << " error: multiple object id members" << endl;

            cerr << m_->file () << ":" << m_->line () << ":" << m_->column ()
                 << ": info: previous id member declared here" << endl;

            valid_ = false;
          }
          else
            m_ = &m;
        }
      }

      bool& valid_;
      semantics::data_member*& m_;
    };

    bool object_;
    member member_;
    traversal::names names_;
    traversal::inherits inherits_;
  };

  //
  //
  struct class_: traversal::class_
  {
    class_ (bool& valid, semantics::unit& unit)
        : valid_ (valid), unit_ (unit), member_ (valid)
    {
      *this >> names_ >> member_;
    }


    virtual void
    traverse (type& c)
    {
      if (c.count ("object"))
        traverse_object (c);
      else if (context::comp_value (c))
        traverse_value (c);
    }

    virtual void
    traverse_object (type& c)
    {
      bool base (false);

      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (b.count ("object"))
          base = true;
        if (context::comp_value (b))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is a value type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: object types cannot derive from value types" << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (true, valid_, id);
        t.traverse (c);
      }

      if (id == 0)
      {
        if (!context::abstract (c))
        {
          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: no data member designated as object id" << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: use '#pragma db id' to specify object id member"
               << endl;

          valid_ = false;
        }
      }
      else
        c.set ("id-member", id);

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0 && !base)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    virtual void
    traverse_value (type& c)
    {
      bool base (false);

      for (type::inherits_iterator i (c.inherits_begin ());
           i != c.inherits_end ();
           ++i)
      {
        type& b (i->base ());

        if (context::comp_value (b))
          base = true;
        else if (b.count ("object"))
        {
          // @@ Should we use hint here?
          //
          string name (b.fq_name ());

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " error: base class '" << name << "' is an object type"
               << endl;

          cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
               << " info: composite value types cannot derive from object "
               << "types" << endl;

          cerr << b.file () << ":" << b.line () << ":" << b.column () << ":"
               << " info: class '" << name << "' is defined here" << endl;

          valid_ = false;
        }
      }

      // Check id.
      //
      semantics::data_member* id (0);
      {
        id_member t (false, valid_, id);
        t.traverse (c);
      }

      if (id != 0)
      {
        cerr << id->file () << ":" << id->line () << ":" << id->column ()
             << ": error: value type data member cannot be designated as "
             << "object id" << endl;

        valid_ = false;
      }

      // Check members.
      //
      member_.count_ = 0;
      names (c);

      if (member_.count_ == 0 && !base)
      {
        cerr << c.file () << ":" << c.line () << ":" << c.column () << ":"
             << " error: no persistent data members in the class" << endl;

        valid_ = false;
      }
    }

    bool& valid_;
    semantics::unit& unit_;

    data_member member_;
    traversal::names names_;
  };
}

bool validator::
validate (options const&,
          semantics::unit& u,
          semantics::path const&)
{
  bool valid (true);

  traversal::unit unit;
  traversal::defines unit_defines;
  traversal::namespace_ ns;
  class_ c (valid, u);

  unit >> unit_defines >> ns;
  unit_defines >> c;

  traversal::defines ns_defines;

  ns >> ns_defines >> ns;
  ns_defines >> c;

  unit.dispatch (u);

  return valid;
}

validator::
validator ()
{
}
