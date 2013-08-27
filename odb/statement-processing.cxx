// file      : odb/statement-processing.cxx
// copyright : Copyright (c) 2009-2013 Code Synthesis Tools CC
// license   : GNU GPL v2; see accompanying LICENSE file

// Place the statement processing code into a separate source file
// to minimize statically-linked object code size when processing
// is not used.

#include <cassert>

#include <odb/statement.hxx>

using namespace std;

// #define LIBODB_DEBUG_STATEMENT_PROCESSING 1

namespace odb
{
  typedef char_traits<char> traits;
  typedef const void* const* bind_type;

  static inline const void*
  bind_at (size_t i, bind_type bind, size_t bind_skip)
  {
    const char* b (reinterpret_cast<const char*> (bind));
    return *reinterpret_cast<bind_type> (b + i * bind_skip);
  }

  static inline const char*
  find (const char* b, const char* e, char c)
  {
    return traits::find (b, e - b, c);
  }

  static inline const char*
  rfind (const char* b, const char* e, char c)
  {
    for (--e; b != e; --e)
      if (*e == c)
        return e;

    return 0;
  }

  // Iterate over INSERT column/value list, UPDATE SET expression list,
  // or SELECT column/join list.
  //
  // for (const char* b (columns_begin), *e (begin (b, end));
  //      e != 0;
  //      next (b, e, end))
  // {
  //   // b points to the beginning of the value (i.e., one past '(').
  //   // e points one past the end of the value (i.e., to ',', ')', or '\n').
  // }
  //
  // // b points one past the last value.
  //
  static inline const char*
  paren_begin (const char*& b, const char* end)
  {
    // Note that the list may not end with '\n'.

    b++; // Skip '('.
    const char* e (find (b, end, '\n'));
    return (e != 0 ? e : end) - 1; // Skip ',' or ')'.
  }

  static inline void
  paren_next (const char*& b, const char*& e, const char* end)
  {
    if (*e == ',')
    {
      b = e + 2; // Skip past '\n'.
      e = find (b, end, '\n');
      e = (e != 0 ? e : end) - 1; // Skip ',' or ')'.
    }
    else
    {
      b = (e + 1 != end ? e + 2 : end); // Skip past '\n'.
      e = 0;
    }
  }

  static inline const char*
  comma_begin (const char* b, const char* end)
  {
    // Note that the list may not end with '\n'.

    const char* e (find (b, end, '\n'));
    return e != 0 ? e - (*(e - 1) == ',' ? 1 : 0) : end; // Skip ','.
  }

  static inline void
  comma_next (const char*& b, const char*& e, const char* end)
  {
    if (*e == ',')
    {
      b = e + 2; // Skip past '\n'.
      e = find (b, end, '\n');
      e = (e != 0 ? e - (*(e - 1) == ',' ? 1 : 0) : end); // Skip ','.
    }
    else
    {
      b = (e != end ? e + 1 : end); // Skip past '\n'.
      e = 0;
    }
  }

  static inline const char*
  newline_begin (const char* b, const char* end)
  {
    // Note that the list may not end with '\n'.

    const char* e (find (b, end, '\n'));
    return e != 0 ? e : end;
  }

  static inline void
  newline_next (const char*& b,
                const char*& e,
                const char* end,
                const char* prefix,
                size_t prefix_size)
  {
    if (e != end)
      e++; // Skip past '\n'.

    b = e;

    // Do we have another element?
    //
    if (static_cast<size_t> (end - b) > prefix_size &&
        traits::compare (b, prefix, prefix_size) == 0)
    {
      e = find (b, end, '\n');
      if (e == 0)
        e = end;
    }
    else
      e = 0;
  }

  // Note that end must point to the beginning of the list.
  //
  static inline const char*
  newline_rbegin (const char* e, const char* end)
  {
    const char* b (rfind (end, e - 1, '\n'));
    return b != 0 ? b + 1 : end; // Skip past '\n'.
  }

  static inline void
  newline_rnext (const char*& b, const char*& e, const char* end)
  {
    if (b != end)
    {
      e = b - 1; // Skip to previous '\n'.
      b = rfind (end, e - 1, '\n');
      b = (b != 0 ? b + 1 : end); // Skip past '\n'.
    }
    else
    {
      e = end - 1; // One before the first element.
      b = 0;
    }
  }

  // Fast path: just remove the "structure".
  //
  static inline void
  process_fast (const char* s, string& r)
  {
    r = s;
    for (size_t i (r.find ('\n')); i != string::npos; i = r.find ('\n', i))
      r[i++] = ' ';
  }

  void statement::
  process_insert (const char* s,
                  bind_type bind,
                  size_t bind_size,
                  size_t bind_skip,
                  char param_symbol,
                  string& r)
  {
#ifndef LIBODB_DEBUG_STATEMENT_PROCESSING
    assert (bind_size != 0); // Cannot be versioned.
#endif

    bool fast (true); // Fast case (if all present).
    for (size_t i (0); i != bind_size && fast; ++i)
    {
      if (bind_at (i, bind, bind_skip) == 0)
        fast = false;
    }

    // Fast path: just remove the "structure".
    //
#ifndef LIBODB_DEBUG_STATEMENT_PROCESSING
    if (fast)
    {
      process_fast (s, r);
      return;
    }
#endif

    // Scan the statement and store the positions of various parts.
    //
    size_t n (traits::length (s));
    const char* e (s + n);

    // Header.
    //
    const char* p (find (s, e, '\n'));
    assert (p != 0);
    size_t header_size (p - s);
    p++;

    // Column list.
    //
    const char* columns_begin (0);
    if (*p == '(')
    {
      columns_begin = p;

      // Find the end of the column list.
      //
      for (const char* ce (paren_begin (p, e)); ce != 0; paren_next (p, ce, e))
        ;
    }

    // OUTPUT
    //
    const char* output_begin (0);
    size_t output_size (0);
    if (e - p > 7 && traits::compare (p, "OUTPUT ", 7) == 0)
    {
      output_begin = p;
      p += 7;
      p = find (p, e, '\n');
      assert (p != 0);
      output_size = p - output_begin;
      p++;
    }

    // VALUES or DEFAULT VALUES
    //
    bool empty (true); // DEFAULT VALUES case (if none present).
    const char* values_begin (0);
    if (e - p > 7 && traits::compare (p, "VALUES\n", 7) == 0)
    {
      p += 7;
      values_begin = p;

      size_t bi (0);
      for (const char* ve (paren_begin (p, e)); ve != 0; paren_next (p, ve, e))
      {
        // We cannot be empty if we have a non-parameterized value, e.g.,
        // INSERT ... VALUES(1,?). We also cannot be empty if this value
        // is present in the bind array.
        //
        if (find (p, ve, param_symbol) == 0 ||
            bind_at (bi++, bind, bind_skip) != 0)
          empty = false;
      }
    }
    else
    {
      // Must be DEFAULT VALUES.
      //
      assert (traits::compare (p, "DEFAULT VALUES", 14) == 0);
      p += 14;

      if (*p == '\n')
        p++;
    }

    // Trailer.
    //
    const char* trailer_begin (0);
    size_t trailer_size (0);
    if (e - p != 0)
    {
      trailer_begin = p;
      trailer_size = e - p;
    }

    // Empty case.
    //
    if (empty)
    {
      r.reserve (header_size +
                 (output_size == 0 ? 0 : output_size + 1) +
                 15 + // For " DEFAULT VALUES"
                 (trailer_size == 0 ? 0 : trailer_size + 1));

      r.assign (s, header_size);

      if (output_size != 0)
      {
        r += ' ';
        r.append (output_begin, output_size);
      }

      r += " DEFAULT VALUES";

      if (trailer_size != 0)
      {
        r += ' ';
        r.append (trailer_begin, trailer_size);
      }

      return;
    }

    // Assume the same size as the original. It can only shrink, and in
    // most cases only slightly. So this is a good approximation.
    //
    r.reserve (n);
    r.assign (s, header_size);

    // Column list.
    //
    {
      r += ' ';

      size_t i (0), bi (0);

      for (const char *c (columns_begin), *ce (paren_begin (c, e)),
                      *v (values_begin), *ve (paren_begin (v, e));
           ce != 0; paren_next (c, ce, e), paren_next (v, ve, e))
      {
        // See if the value contains the parameter symbol and, if so,
        // whether it is present in the bind array.
        //
        if (find (v, ve, param_symbol) != 0 &&
            bind_at (bi++, bind, bind_skip) == 0)
          continue;

        // Append the column.
        //
        if (i++ == 0)
          r += '(';
        else
          r += ", "; // Add the space for consistency with the fast path.

        r.append (c, ce - c);
      }

      r += ')';
    }

    // OUTPUT
    //
    if (output_size != 0)
    {
      r += ' ';
      r.append (output_begin, output_size);
    }

    // Value list.
    //
    {
      r += " VALUES ";

      size_t i (0), bi (0);

      for (const char* v (values_begin), *ve (paren_begin (v, e));
           ve != 0; paren_next (v, ve, e))
      {
        // See if the value contains the parameter symbol and, if so,
        // whether it is present in the bind array.
        //
        if (find (v, ve, param_symbol) != 0 &&
            bind_at (bi++, bind, bind_skip) == 0)
          continue;

        // Append the value.
        //
        if (i++ == 0)
          r += '(';
        else
          r += ", "; // Add the space for consistency with the fast path.

        r.append (v, ve - v);
      }

      r += ')';
    }

    // Trailer.
    //
    if (trailer_size != 0)
    {
      r += ' ';
      r.append (trailer_begin, trailer_size);
    }
  }

  void statement::
  process_update (const char* s,
                  bind_type bind,
                  size_t bind_size,
                  size_t bind_skip,
                  char param_symbol,
                  string& r)
  {
    bool fast (true); // Fast case (if all present).
    for (size_t i (0); i != bind_size && fast; ++i)
    {
      if (bind_at (i, bind, bind_skip) == 0)
        fast = false;
    }

    // Fast path: just remove the "structure".
    //
#ifndef LIBODB_DEBUG_STATEMENT_PROCESSING
    if (fast)
    {
      process_fast (s, r);
      return;
    }
#endif

    // Scan the statement and store the positions of various parts.
    //
    size_t n (traits::length (s));
    const char* e (s + n);

    // Header.
    //
    const char* p (find (s, e, '\n'));
    assert (p != 0);
    size_t header_size (p - s);
    p++;

    // SET
    //
    bool empty (true); // Empty SET case.
    const char* set_begin (0);
    if (e - p > 4 && traits::compare (p, "SET\n", 4) == 0)
    {
      p += 4;
      set_begin = p;

      // Scan the SET list.
      //
      size_t bi (0);
      for (const char* pe (comma_begin (p, e)); pe != 0; comma_next (p, pe, e))
      {
        if (empty)
        {
          // We cannot be empty if we have a non-parameterized set expression,
          // e.g., UPDATE ... SET ver=ver+1 ... We also cannot be empty if
          // this expression is present in the bind array.
          //
          if (find (p, pe, param_symbol) == 0 ||
              bind_at (bi++, bind, bind_skip) != 0)
            empty = false;
        }
      }
    }

    // Trailer.
    //
    const char* trailer_begin (0);
    size_t trailer_size (0);
    if (e - p != 0)
    {
      trailer_begin = p;
      trailer_size = e - p;
    }

    // Empty case.
    //
    if (empty)
    {
      r.reserve (header_size + (trailer_size == 0 ? 0 : trailer_size + 1));
      r.assign (s, header_size);

      if (trailer_size != 0)
      {
        r += ' ';
        r.append (trailer_begin, trailer_size);
      }

      return;
    }

    // Assume the same size as the original. It can only shrink, and in
    // most cases only slightly. So this is a good approximation.
    //
    r.reserve (n);
    r.assign (s, header_size);

    // SET list.
    //
    {
      r += " SET ";

      size_t i (0), bi (0);

      for (const char* p (set_begin), *pe (comma_begin (p, e));
           pe != 0; comma_next (p, pe, e))
      {
        // See if the value contains the parameter symbol and, if so,
        // whether it is present in the bind array.
        //
        if (find (p, pe, param_symbol) != 0 &&
            bind_at (bi++, bind, bind_skip) == 0)
          continue;

        // Append the expression.
        //
        if (i++ != 0)
          r += ", "; // Add the space for consistency with the fast path.

        r.append (p, pe - p);
      }
    }

    // Trailer.
    //
    if (trailer_size != 0)
    {
      r += ' ';
      r.append (trailer_begin, trailer_size);
    }
  }

  void statement::
  process_select (const char* s,
                  bind_type bind,
                  size_t bind_size,
                  size_t bind_skip,
                  char quote_open,
                  char quote_close,
#ifndef LIBODB_DEBUG_STATEMENT_PROCESSING
                  bool optimize,
#else
                  bool,
#endif
                  string& r)
  {
    bool fast (true); // Fast case (if all present).
    for (size_t i (0); i != bind_size && fast; ++i)
    {
      if (bind_at (i, bind, bind_skip) == 0)
        fast = false;
    }

    // Fast path: just remove the "structure".
    //
#ifndef LIBODB_DEBUG_STATEMENT_PROCESSING
    if (fast && !optimize)
    {
      process_fast (s, r);
      return;
    }
#endif

    // Scan the statement and store the positions of various parts.
    //
    size_t n (traits::length (s));
    const char* e (s + n);

    // Header.
    //
    const char* p (find (s, e, '\n'));
    assert (p != 0);
    size_t header_size (p - s);
    p++;

    // Column list.
    //
    const char* columns_begin (p);
    for (const char* ce (comma_begin (p, e)); ce != 0; comma_next (p, ce, e))
      ;

    // FROM.
    assert (traits::compare (p, "FROM ", 5) == 0);
    const char* from_begin (p);
    p = find (p, e, '\n'); // May not end with '\n'.
    if (p == 0)
      p = e;
    size_t from_size (p - from_begin);
    if (p != e)
      p++;


    // JOIN list.
    //
    const char* joins_begin (0), *joins_end (0);
    if (e - p > 5 && traits::compare (p, "LEFT JOIN ", 10) == 0)
    {
      joins_begin = p;

      // Find the end of the JOIN list.
      //
      for (const char* je (newline_begin (p, e));
           je != 0; newline_next (p, je, e, "LEFT JOIN ", 10))
        ;

      joins_end = (p != e ? p - 1 : p);
    }

#ifndef LIBODB_DEBUG_STATEMENT_PROCESSING
    if (fast && joins_begin == 0)
    {
      // No JOINs to optimize so can still take the fast path.
      //
      process_fast (s, r);
      return;
    }
#endif

    // Trailer (WHERE, ORDER BY, etc).
    //
    const char* trailer_begin (0);
    size_t trailer_size (0);
    if (e - p != 0)
    {
      trailer_begin = p;
      trailer_size = e - p;
    }

    // Assume the same size as the original. It can only shrink, and in
    // most cases only slightly. So this is a good approximation.
    //
    r.reserve (n);
    r.assign (s, header_size);

    // Column list.
    //
    {
      r += ' ';

      size_t i (0), bi (0);

      for (const char *c (columns_begin), *ce (comma_begin (c, e));
           ce != 0; comma_next (c, ce, e))
      {
        // See if the column is present in the bind array.
        //
        if (bind_at (bi++, bind, bind_skip) == 0)
          continue;

        // Append the column.
        //
        if (i++ != 0)
          r += ", "; // Add the space for consistency with the fast path.

        r.append (c, ce - c);
      }
    }

    // From.
    //
    r += ' ';
    r.append (from_begin, from_size);

    // JOIN list, pass 1.
    //
    size_t join_pos (0);
    if (joins_begin != 0)
    {
      // Fill in the JOIN "area" with spaces.
      //
      r.resize (r.size () + joins_end - joins_begin + 1, ' ');
      join_pos = r.size () + 1; // End of the last JOIN.
    }

    // Trailer.
    //
    if (trailer_size != 0)
    {
      r += ' ';
      r.append (trailer_begin, trailer_size);
    }

    // JOIN list, pass 2.
    //
    if (joins_begin != 0)
    {
      // Splice the JOINs into the pre-allocated area.
      //
      for (const char* je (joins_end), *j (newline_rbegin (je, joins_begin));
           j != 0; newline_rnext (j, je, joins_begin))
      {
        size_t n (je - j);

        // Get the alias or, if none used, the table name.
        //
        p = find (j + 10, je, ' '); // 10 for "LEFT JOIN ".
        const char* table_end (p);
        p++; // Skip space.

        const char* alias_begin (0);
        size_t alias_size (0);
        if (je - p > 3 && traits::compare (p, "AS ", 3) == 0)
        {
          p += 3;
          alias_begin = p;
          alias_size = find (p, je, ' ') - alias_begin;
        }
        else
        {
          alias_begin = j + 10;
          alias_size = table_end - alias_begin;
        }

        // The alias must be quoted.
        //
        assert (*alias_begin == quote_open &&
                *(alias_begin + alias_size - 1) == quote_close);

        // We now need to see if the alias is used in either the SELECT
        // list, the WHERE conditions, or the ON condition of any of the
        // JOINs that we have already processed and decided to keep.
        //
        // Instead of re-parsing the whole thing again, we are going to
        // take a shortcut and simply search for the alias in the statement
        // we have constructed so far (that's why we have have added the
        // trailer before filling in the JOINs). To make it more robust,
        // we are going to do a few extra sanity checks, specifically,
        // that the alias is a top level identifier and is followed by
        // only a single identifer (column). This will catch cases like
        // [s].[t].[c] where [s] is also used as an alias or LEFT JOIN [t]
        // where [t] is also used as an alias in another JOIN.
        //
        bool found (false);
        for (size_t p (r.find (alias_begin, 0, alias_size));
             p != string::npos;
             p = r.find (alias_begin, p + alias_size, alias_size))
        {
          size_t e (p + alias_size);

          // If we are not a top-level qualifier or not a bottom-level,
          // then we are done (3 is for at least "[a]").
          //
          if ((p != 0 && r[p - 1] == '.') ||
              (e + 3 >= r.size () || (r[e] != '.' || r[e + 1] != quote_open)))
            continue;

          // The only way to distinguish the [a].[c] from FROM [a].[c] or
          // LEFT JOIN [a].[c] is by checking the prefix.
          //
          if ((p > 5 && r.compare (p - 5, 5, "FROM ") == 0) ||
              (p > 10 && r.compare (p - 10, 10, "LEFT JOIN ") == 0))
            continue;

          // Check that we are followed by a single identifier.
          //
          e = r.find (quote_close, e + 2);
          if (e == string::npos || (e + 1 != r.size () && r[e + 1] == '.'))
            continue;

          found = true;
          break;
        }

        join_pos -= n + 1; // Extra one for space.
        if (found)
          r.replace (join_pos, n, j, n);
        else
          r.erase (join_pos - 1, n + 1); // Extra one for space.
      }
    }
  }
}