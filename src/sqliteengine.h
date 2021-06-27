#pragma once

#include "sqlite3.h"
#include <vector>
#include <optional>
#include <string>
#include <sstream>
#include <tuple>
#include <algorithm>
#include <functional>
#include <numeric>

//-----------------------------------------------------------------------------------
using Blob = std::vector<std::uint8_t>;
//-----------------------------------------------------------------------------------
enum class SqlColKind{REGULAR, AUTO_PRIMARY_KEY, UNIQUE};
template <typename V>
struct SqlCol
{
  using ReadType = V;
  const std::string name;
  SqlColKind const kind = SqlColKind::REGULAR;
};
template <typename T, typename V>
struct SqlColMap
{
  using ReadType = typename SqlCol<V>::ReadType;
  SqlCol<V> info;
  V T::*structPointer;
  V& memberPtr(T& t){return t.*structPointer;}
  V const& memberPtr(T const& t)const{return t.*structPointer;}
};
//-----------------------------------------------------------------------------------
using SqlInteger = SqlCol<int>;
//-----------------------------------------------------------------------------------
using SqlText = SqlCol<std::string>;
//-----------------------------------------------------------------------------------
using SqlBlob = SqlCol<Blob>;
//-----------------------------------------------------------------------------------
template <typename T>
struct SqlColType;
template<>
struct SqlColType<SqlInteger>
{
  static inline const char* colType(){ return "INTEGER";}
};
template <>
struct SqlColType<SqlText>
{
  static inline const char* colType(){ return "TEXT";}
};
template <>
struct SqlColType<SqlBlob>
{
  static inline const char* colType(){ return "BLOB";}
};
template <typename SQLCOL>
static inline const char* colType(){ return SqlColType<SQLCOL>::colType();}
template <typename V>
static inline const char* colType(SqlCol<V> const& map){ return colType<SqlCol<V>>();}
template <typename T, typename V>
static inline const char* colType(SqlColMap<T, V> const& map){ return colType<SqlCol<V>>();}
//-----------------------------------------------------------------------------------
template <typename T>
using SqlColReadType = typename std::decay_t<std::remove_reference_t<T>>::ReadType;
template <typename ...SqlCol>
using SqlColsReadType = std::tuple<typename SqlCol::ReadType...>;
//-----------------------------------------------------------------------------------
///@return table name holding Record entries
template<typename Record>
const char* SQLiteTable();
//-----------------------------------------------------------------------------------
///@return std::tuple of SqlCol<Record, V>
template<typename Record>
auto SQLiteColumns();
//-----------------------------------------------------------------------------------
class SQLiteStorageEngineBase
{
public:
  struct Result
  {
    std::string error;
    operator bool()const{return error.empty();}
  };

  SQLiteStorageEngineBase(std::string const& dbPath);
  ~SQLiteStorageEngineBase();

  enum class Mode{Read, ReadWrite, ReadWriteCreate};
  Result open(Mode mode = Mode::Read);
  void close();

  operator bool() const;
protected:

  std::string m_dbPath;
  sqlite3 *m_db = nullptr;
  bool m_opened = false;
};
//-----------------------------------------------------------------------------------
class SQLiteStorageViewEngine : public SQLiteStorageEngineBase
{
public:

  SQLiteStorageViewEngine(std::string const& dbPath,
                          Mode mode = Mode::Read);

  struct Filter
  {
    std::string col, value;
  };
  bool setFilter(std::optional<Filter> const& filter);
  struct Sorting
  {
    std::string col;
    bool ascending;
  };
  enum class SortingOption{ASCENDING, DESCENDING};
  template <typename T, typename V>
  void setSortingColumn(V T::*structPointer, SortingOption sorting);
  template <typename T, typename V>
  std::optional<SQLiteStorageViewEngine::SortingOption>
  isSortingColumn(V T::*structPointer);

  std::optional<Sorting> const& sorting()const{return m_sorting;}

  template <typename T>
  std::vector<T> readTable(std::vector<Filter> const& filters = {},
                           std::optional<Sorting> const& sorting = std::nullopt,
                           std::optional<int> iStart = std::nullopt,
                           std::optional<int> count = std::nullopt)const;
  template <typename T, typename Tuple>
  std::vector<T> readTable(std::string const& name, Tuple const& cols,
                           std::vector<Filter> const& filters = {},
                           std::optional<Sorting> const& sorting = std::nullopt,
                           std::optional<int> iStart = std::nullopt,
                           std::optional<int> count = std::nullopt)const;
  /// @tparam OP: void(T && record)
  template <typename T, typename Tuple, typename OP>
  Result readTable(std::string const& name, Tuple const& cols,
                 OP const& recordHandler,
                 std::vector<Filter> const& filters = {},
                 std::optional<Sorting> const& sorting = std::nullopt,
                 std::optional<int> iStart = std::nullopt,
                 std::optional<int> count = std::nullopt)const;
  /// @tparam OP: void(T && record)
  template <typename T, typename OP>
  Result readTable(OP const& recordHandler,
                 std::vector<Filter> const& filters = {},
                 std::optional<Sorting> const& sorting = std::nullopt,
                 std::optional<int> iStart = std::nullopt,
                 std::optional<int> count = std::nullopt)const;

  int countTable(std::string const& table, std::vector<Filter> const& filters);

private:
  std::optional<Sorting> m_sorting = std::nullopt;
};

//-----------------------------------------------------------------------------------
std::string join( std::vector<std::string> const& fields,
                  std::string const& delimiter=",");

//-----------------------------------------------------------------------------------
enum class SqlInserterMode{ADD, UPDATE};
//-----------------------------------------------------------------------------------
template <typename T>
class SqlInserter
{
public:
  using Result = SQLiteStorageEngineBase::Result;
  using Mode = SqlInserterMode;
  SqlInserter(sqlite3 *db, Mode mode);

  void insert(T const& entry);
  Result exec();

private:
  Mode m_mode;
  sqlite3 *m_db;
  sqlite3_stmt* m_stmt = nullptr;
};
//-----------------------------------------------------------------------------------

class SQLiteStorageModificationEngine : public SQLiteStorageViewEngine
{
public:

  SQLiteStorageModificationEngine(std::string const& dbPath);

  template <typename T>
  SqlInserter<T> buildInserter(SqlInserterMode mode);
  template<typename SelV, typename TgtV>
  Result updateSelectedValues(
        std::string const&table,
        SqlCol<SelV> const&selectCol,
        std::vector<SelV>const& selectValues,
        SqlCol<TgtV> const&targetCol,
        TgtV const& newValue);
  template<typename SelV>
  Result deleteRecords(
        std::string const&table,
        SqlCol<SelV> const&selectCol,
        std::vector<SelV>const& selectValues);
protected:

  SQLiteStorageModificationEngine(std::string const& dbPath,
                                  Mode mode);
};

//-----------------------------------------------------------------------------------

class SQLiteStorageCreationEngine : public SQLiteStorageModificationEngine
{
public:

  SQLiteStorageCreationEngine(std::string const& dbPath);

  template <typename T>
  Result createTable(std::string const& additional = "");
};
