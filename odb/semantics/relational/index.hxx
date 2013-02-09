// file      : odb/semantics/relational/index.hxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_SEMANTICS_RELATIONAL_INDEX_HXX
#define ODB_SEMANTICS_RELATIONAL_INDEX_HXX

#include <odb/semantics/relational/elements.hxx>
#include <odb/semantics/relational/key.hxx>

namespace semantics
{
  namespace relational
  {
    // Note that in our model indexes are defined in the table scope.
    //
    class index: public key
    {
    public:
      index (string const& id,
             string const& t = string (),
             string const& m = string (),
             string const& o = string ())
          : key (id), type_ (t), method_ (m), options_ (o)
      {
      }

      string const&
      type () const
      {
        return type_;
      }

      string const&
      method () const
      {
        return method_;
      }

      string const&
      options () const
      {
        return options_;
      }

      virtual string
      kind () const
      {
        return "index";
      }

    private:
      string type_;    // E.g., "UNIQUE", etc.
      string method_;  // E.g., "BTREE", etc.
      string options_; // Database-specific index options.
    };
  }
}

#endif // ODB_SEMANTICS_RELATIONAL_INDEX_HXX
