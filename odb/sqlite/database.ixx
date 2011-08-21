// file      : odb/sqlite/database.ixx
// author    : Boris Kolpackov <boris@codesynthesis.com>
// copyright : Copyright (c) 2009-2011 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

namespace odb
{
  namespace sqlite
  {
    inline connection_ptr database::
    connection ()
    {
      // Go through the virtual connection_() function instead of
      // directly to allow overriding.
      //
      return connection_ptr (
        static_cast<sqlite::connection*> (connection_ ()));
    }

    inline transaction_impl* database::
    begin ()
    {
      return connection ()->begin ();
    }

    inline transaction_impl* database::
    begin_immediate ()
    {
      return connection ()->begin_immediate ();
    }

    inline transaction_impl* database::
    begin_exclusive ()
    {
      return connection ()->begin_exclusive ();
    }
  }
}
