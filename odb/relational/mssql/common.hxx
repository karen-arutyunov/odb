// file      : odb/relational/mssql/common.hxx
// copyright : Copyright (c) 2009-2012 Code Synthesis Tools CC
// license   : GNU GPL v3; see accompanying LICENSE file

#ifndef ODB_RELATIONAL_MSSQL_COMMON_HXX
#define ODB_RELATIONAL_MSSQL_COMMON_HXX

#include <odb/relational/common.hxx>
#include <odb/relational/mssql/context.hxx>

namespace relational
{
  namespace mssql
  {
    struct member_base: virtual relational::member_base, context
    {
      member_base (base const& x): base (x) {}

      // This c-tor is for the direct use inside the mssql namespace.
      // If you do use this c-tor, you should also explicitly call
      // relational::member_base.
      //
      member_base () {}

      virtual void
      traverse (semantics::data_member& m);

      struct member_info
      {
        semantics::data_member& m; // Member.
        semantics::type& t;        // Cvr-unqualified member C++ type, note
                                   // that m.type () may not be the same as t.
        semantics::type* wrapper;  // Wrapper type if member is a composite or
                                   // container wrapper, also cvr-unqualified.
                                   // In this case t is the wrapped type.
        bool cq;                   // True if the original (wrapper) type
                                   // is const-qualified.
        sql_type const* st;        // Member SQL type (only simple values).
        string& var;               // Member variable name with trailing '_'.

        // C++ type fq-name.
        //
        string
        fq_type (bool unwrap = true) const
        {
          semantics::names* hint;

          if (wrapper != 0 && unwrap)
          {
            // Use the hint from the wrapper unless the wrapped type
            // is qualified.
            //
            hint = wrapper->get<semantics::names*> ("wrapper-hint");
            utype (*context::wrapper (*wrapper), hint);
            return t.fq_name (hint);
          }

          // Use the original type from 'm' instead of 't' since the hint
          // may be invalid for a different type. Plus, if a type is
          // overriden, then the fq_type must be as well.
          //
          if (fq_type_.empty ())
          {
            semantics::type& t (utype (m, hint));
            return t.fq_name (hint);
          }
          else
            return fq_type_;
        }

        string const& fq_type_;

        member_info (semantics::data_member& m_,
                     semantics::type& t_,
                     semantics::type* wrapper_,
                     bool cq_,
                     string& var_,
                     string const& fq_type)
            : m (m_),
              t (t_),
              wrapper (wrapper_),
              cq (cq_),
              st (0),
              var (var_),
              fq_type_ (fq_type)
        {
        }
      };

      bool
      container (member_info& mi)
      {
        // This cannot be a container if we have a type override.
        //
        return type_override_ == 0 && context::container (mi.m);
      }

      // The false return value indicates that no further callbacks
      // should be called for this member.
      //
      virtual bool
      pre (member_info&)
      {
        return true;
      }

      virtual void
      post (member_info&)
      {
      }

      virtual void
      traverse_composite (member_info&)
      {
      }

      virtual void
      traverse_container (member_info&)
      {
      }

      virtual void
      traverse_object_pointer (member_info& mi)
      {
        traverse_simple (mi);
      }

      virtual void
      traverse_simple (member_info&);

      virtual void
      traverse_integer (member_info&)
      {
      }

      virtual void
      traverse_decimal (member_info&)
      {
      }

      virtual void
      traverse_smallmoney (member_info&)
      {
      }

      virtual void
      traverse_money (member_info&)
      {
      }

      virtual void
      traverse_float4 (member_info&)
      {
      }

      virtual void
      traverse_float8 (member_info&)
      {
      }

      virtual void
      traverse_string (member_info&)
      {
      }

      virtual void
      traverse_long_string (member_info&)
      {
      }

      virtual void
      traverse_nstring (member_info&)
      {
      }

      virtual void
      traverse_long_nstring (member_info&)
      {
      }

      virtual void
      traverse_binary (member_info&)
      {
      }

      virtual void
      traverse_long_binary (member_info&)
      {
      }

      virtual void
      traverse_date (member_info&)
      {
      }

      virtual void
      traverse_time (member_info&)
      {
      }

      virtual void
      traverse_datetime (member_info&)
      {
      }

      virtual void
      traverse_datetimeoffset (member_info&)
      {
      }

      virtual void
      traverse_uniqueidentifier (member_info&)
      {
      }

      virtual void
      traverse_rowversion (member_info&)
      {
      }
    };

    struct member_image_type: member_base
    {
      member_image_type (semantics::type* type = 0,
                         string const& fq_type = string (),
                         string const& key_prefix = string ());
      string
      image_type (semantics::data_member&);

      virtual void
      traverse_composite (member_info&);

      virtual void
      traverse_integer (member_info&);

      virtual void
      traverse_decimal (member_info&);

      virtual void
      traverse_smallmoney (member_info&);

      virtual void
      traverse_money (member_info&);

      virtual void
      traverse_float4 (member_info&);

      virtual void
      traverse_float8 (member_info&);

      virtual void
      traverse_string (member_info&);

      virtual void
      traverse_long_string (member_info&);

      virtual void
      traverse_nstring (member_info&);

      virtual void
      traverse_long_nstring (member_info&);

      virtual void
      traverse_binary (member_info&);

      virtual void
      traverse_long_binary (member_info&);

      virtual void
      traverse_date (member_info&);

      virtual void
      traverse_time (member_info&);

      virtual void
      traverse_datetime (member_info&);

      virtual void
      traverse_datetimeoffset (member_info&);

      virtual void
      traverse_uniqueidentifier (member_info&);

      virtual void
      traverse_rowversion (member_info&);

    private:
      string type_;
    };

    struct member_database_type_id: member_base
    {
      member_database_type_id (semantics::type* type = 0,
                               string const& fq_type = string (),
                               string const& key_prefix = string ());
      string
      database_type_id (type&);

      virtual void
      traverse_composite (member_info&);

      virtual void
      traverse_integer (member_info&);

      virtual void
      traverse_decimal (member_info&);

      virtual void
      traverse_smallmoney (member_info&);

      virtual void
      traverse_money (member_info&);

      virtual void
      traverse_float4 (member_info&);

      virtual void
      traverse_float8 (member_info&);

      virtual void
      traverse_string (member_info&);

      virtual void
      traverse_long_string (member_info&);

      virtual void
      traverse_nstring (member_info&);

      virtual void
      traverse_long_nstring (member_info&);

      virtual void
      traverse_binary (member_info&);

      virtual void
      traverse_long_binary (member_info&);

      virtual void
      traverse_date (member_info&);

      virtual void
      traverse_time (member_info&);

      virtual void
      traverse_datetime (member_info&);

      virtual void
      traverse_datetimeoffset (member_info&);

      virtual void
      traverse_uniqueidentifier (member_info&);

      virtual void
      traverse_rowversion (member_info&);

    private:
      string type_id_;
    };

    struct has_long_data: object_columns_base, context
    {
      has_long_data (bool& r): r_ (r) {}

      virtual bool
      traverse_column (semantics::data_member& m, string const&, bool)
      {
        if (inverse (m))
          return false;

        sql_type const& st (column_sql_type (m));

        switch (st.type)
        {
        case sql_type::CHAR:
        case sql_type::VARCHAR:
          {
            // Zero precision means max in VARCHAR(max).
            //
            if (st.prec == 0 || st.prec > options.mssql_short_limit ())
              r_ = true;

            break;
          }
        case sql_type::NCHAR:
        case sql_type::NVARCHAR:
          {
            // Zero precision means max in NVARCHAR(max). Note that
            // the precision is in 2-byte UCS-2 characters, not bytes.
            //
            if (st.prec == 0 || st.prec * 2 > options.mssql_short_limit ())
              r_ = true;

            break;
          }
        case sql_type::BINARY:
        case sql_type::VARBINARY:
          {
            // Zero precision means max in VARCHAR(max).
            //
            if (st.prec == 0 || st.prec > options.mssql_short_limit ())
              r_ = true;

            break;
          }
        case sql_type::TEXT:
        case sql_type::NTEXT:
        case sql_type::IMAGE:
          {
            r_ = true;
            break;
          }
        default:
          break;
        }

        return true;
      }

    private:
      bool& r_;
    };
  }
}
#endif // ODB_RELATIONAL_MSSQL_COMMON_HXX
