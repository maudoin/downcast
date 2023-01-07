#pragma once

#include "sqliteengine.h"
#include <vector>
#include <optional>
#include <string>
#include <sstream>
#include <tuple>
#include <algorithm>
#include <numeric>

template <typename T, typename V>
SqlColMap<T, V> map(SqlCol<V> info, V T::*structPointer)
{
  return {info, structPointer};
}
//-----------------------------------------------------------------------------------
template <typename ...SqlCol>
std::vector<std::string> columnLabels(SqlCol&&... sqlCol)
{
  return {sqlCol.info.name ...}  ;
}
//-----------------------------------------------------------------------------------
template <typename T, typename OP, typename ColsTuple=decltype (SQLiteColumns<T>())>
void forEachColumn(OP const& op, ColsTuple const& cols=SQLiteColumns<T>())
{
  std::apply([&](auto&& ...sqlCol){(op(std::forward<decltype(sqlCol)>(sqlCol)),...);}, cols);
}
//-----------------------------------------------------------------------------------
template <typename T, typename ColsTuple=decltype (SQLiteColumns<T>())>
std::optional<std::string> getColName(int targetIndex, ColsTuple const& cols=SQLiteColumns<T>())
{
  int i=0;
  std::optional<std::string> name;
  forEachColumn<T>([&](auto const& col)
  {
    if(i==targetIndex)
    {
      name.emplace(col.info.name);
    }
    ++i;
  }, cols);
  return name;
}
//-----------------------------------------------------------------------------------
template <typename T, typename V>
std::optional<int> columnIndexOf(V T::*structPointer)
{
  std::optional<int> index;
  int i=0;
  forEachColumn<T>([&](auto const& col)
  {
    if constexpr(std::is_same_v<V, SqlColReadType<decltype (col)>>)
    {
      if(col.structPointer==structPointer)
      {
        index.emplace(i);
      }
    }
    ++i;
  });
  return index;
}
//-----------------------------------------------------------------------------------
template <typename T>
SqlInserter<T>::SqlInserter(sqlite3 *db, Mode mode)
  :m_mode(mode)
  ,m_db(db)
{
  char* errorMessage;
  sqlite3_exec(m_db, "BEGIN TRANSACTION", NULL, NULL, &errorMessage);
  std::ostringstream oss;
  oss << "INSERT";
  switch(mode)
  {
  case Mode::ADD:
    oss << " OR IGNORE";
    break;
  case Mode::UPDATE:
    oss << " OR REPLACE";
    break;
  }
  oss << " INTO " << SQLiteTable<T>() << " (";

  bool hasPrevious = false;
  forEachColumn<T>([&](auto sqlCol)
  {
    if(mode == Mode::UPDATE || sqlCol.info.kind != SqlColKind::AUTO_PRIMARY_KEY)
    {
      if(hasPrevious)
      {
        oss << ",";
      }
      oss << sqlCol.info.name;
      hasPrevious = true;
    }
  });

  oss <<" ) VALUES (";

  hasPrevious = false;
  forEachColumn<T>([&](auto sqlCol)
  {
    if(mode == Mode::UPDATE || sqlCol.info.kind != SqlColKind::AUTO_PRIMARY_KEY)
    {
      if(hasPrevious)
      {
        oss << ",";
      }
      oss << "?";
      hasPrevious = true;
    }
  });

  oss << ")";

  std::string query=oss.str();
  if ( SQLITE_OK != sqlite3_prepare(m_db, query.data(), query.size(), &m_stmt, NULL))
  {
    printf("\nCould not sqlite3_prepare %s.\n", sqlite3_errmsg(m_db));
    abort();return ;
  }
}

//-----------------------------------------------------------------------------------
template <typename T>
void SqlInserter<T>::insert(T const& entry)
{

  int row=1;
  forEachColumn<T>([&](auto sqlCol)
  {
    if(m_mode == Mode::UPDATE || sqlCol.info.kind != SqlColKind::AUTO_PRIMARY_KEY)
    {
      if constexpr(isInstanceOf<SqlInteger>(sqlCol.info))
      {
        sqlite3_bind_int(m_stmt, row++, sqlCol.memberPtr(entry));
        return;
      }
      if constexpr(isInstanceOf<SqlText>(sqlCol.info))
      {
        std::string const& s{sqlCol.memberPtr(entry)};
        sqlite3_bind_text(m_stmt, row++, s.data(), s.size(), SQLITE_TRANSIENT);
        return;
      }
      if constexpr(isInstanceOf<SqlBlob>(sqlCol.info))
      {
        Blob const& s{sqlCol.memberPtr(entry)};
        sqlite3_bind_blob(m_stmt, row++, s.data(), s.size(), SQLITE_TRANSIENT);
        return;
      }
      sqlite3_bind_null(m_stmt, row++);
    }
  });

  auto state = sqlite3_step(m_stmt);
  if ( state!= SQLITE_DONE) {

    printf("\nCould not step (execute) stmt %s.\n", sqlite3_errmsg(m_db));
    return ;
  }

  sqlite3_reset(m_stmt);
}

//-----------------------------------------------------------------------------------
template <typename T>
typename SqlInserter<T>::Result SqlInserter<T>::exec()
{
  char* errorMessage;
  int rc = sqlite3_exec(m_db, "END TRANSACTION", NULL, NULL, &errorMessage);

  sqlite3_finalize(m_stmt);
  return (rc==SQLITE_OK)?Result{}:Result{{sqlite3_errmsg(m_db)}};
}
//-----------------------------------------------------------------------------------
template <typename T, typename Param>
constexpr bool isInstanceOf(Param&& p)
{
  return std::is_same_v<T, std::decay_t<decltype (p)>>;
}
//-----------------------------------------------------------------------------------
template <typename T, typename V>
void SQLiteStorageViewEngine::setSortingColumn(V T::*structPointer, SortingOption sorting)
{
  if(std::optional<int> newIndex = columnIndexOf(structPointer))
  {
    if(auto name = getColName<T>(*newIndex))
    {
      m_sorting.emplace(Sorting{*name, sorting==SortingOption::ASCENDING});
    }
    else
    {
      m_sorting.reset();
    }
  }
  else
  {
    m_sorting.reset();
  }
}
//-----------------------------------------------------------------------------------
template <typename T, typename V>
std::optional<SQLiteStorageViewEngine::SortingOption>
SQLiteStorageViewEngine::isSortingColumn(V T::*structPointer)
{
  if(std::optional<int> newIndex = columnIndexOf(structPointer))
  {
    if(m_sorting && m_sorting->col==getColName<T>(*newIndex))
    {
      return std::make_optional<SortingOption>(
            m_sorting->ascending?
              SortingOption::ASCENDING:
              SortingOption::DESCENDING);
    }
  }
  return std::nullopt;
}
//-----------------------------------------------------------------------------------
template <typename T, typename Tuple, typename Op>
SQLiteStorageViewEngine::Result
SQLiteStorageViewEngine::readTable(
    std::string const& name, Tuple const& cols,
    Op const& recordHandler,
    std::vector<SQLiteStorageViewEngine::Filter> const& filters,
    std::optional<Sorting> const& sorting,
    std::optional<int> iStart,
    std::optional<int> count)const
{
  std::ostringstream oss;
  oss << "SELECT " << join(std::apply([](auto&& ...sqlCol){return columnLabels(std::forward<decltype(sqlCol)>(sqlCol)...);},cols)) << " FROM " <<name;
  if(filters.size()>0)
  {
    oss << " WHERE " << filters.front().col << " = "<< filters.front().value;
    std::for_each(filters.begin()+1, filters.end(),
                  [&](SQLiteStorageViewEngine::Filter const& f)
    {
      oss << " AND "<< f.col << " = "<< f.value;
    });
  }
  if(sorting)
  {
    oss << " ORDER BY "<<sorting->col
        << (sorting->ascending?" ASC":" DESC");
  }
  if(count)
  {
    oss <<  " LIMIT "<<*count;
  }
  if(iStart)
  {
    oss <<  " OFFSET "<<*iStart;
  }
  sqlite3_stmt*        stmt;
  std::string query=oss.str();
  sqlite3_prepare(m_db, query.data(), query.size(), &stmt, NULL);

  auto stepState = sqlite3_step (stmt);
  while(stepState == SQLITE_ROW)
  {
    int row=0;
    auto get = [&](auto sqlCol)-> typename std::decay_t<decltype (sqlCol.info)>::ReadType
    {
      if constexpr(isInstanceOf<SqlInteger>(sqlCol.info))
      {
        if(SQLITE_NULL == sqlite3_column_type(stmt, row))
        {
          return 0;
        }
        return sqlite3_column_int(stmt, row++);
      }
      if constexpr(isInstanceOf<SqlText>(sqlCol.info))
      {
        auto text  = sqlite3_column_text(stmt, row++);
        return text?std::string{reinterpret_cast<const char*>(text)}:"";
      }
      if constexpr(isInstanceOf<SqlBlob>(sqlCol.info))
      {
        Blob blob(sqlite3_column_bytes(stmt, row));
        const void * data = sqlite3_column_blob(stmt, row++);
        memcpy(blob.data(), data, blob.size());
        return std::move(blob);
      }
    };
    recordHandler(std::apply([&](auto&& ...sqlCol){return T{get(std::forward<decltype(sqlCol)>(sqlCol))...};}, cols));
    stepState = sqlite3_step (stmt);
  }

  sqlite3_finalize(stmt);
  return (stepState==SQLITE_DONE)?Result{}:Result{{sqlite3_errmsg(m_db)}};
}
//-----------------------------------------------------------------------------------
template <typename T, typename Op>
SQLiteStorageViewEngine::Result
SQLiteStorageViewEngine::readTable(
      Op const& recordHandler,
      std::vector<SQLiteStorageViewEngine::Filter> const& filters,
      std::optional<Sorting> const& sorting,
      std::optional<int> iStart,
      std::optional<int> count)const
{
  return readTable<T>(
        SQLiteTable<T>(), SQLiteColumns<T>(),
        recordHandler,
        filters, sorting, iStart, count);
}
//-----------------------------------------------------------------------------------
template <typename T, typename Tuple>
std::vector<T> SQLiteStorageViewEngine::readTable(
    std::string const& name, Tuple const& cols,
    std::vector<SQLiteStorageViewEngine::Filter> const& filters,
    std::optional<Sorting> const& sorting,
    std::optional<int> iStart,
    std::optional<int> count)const
{
  std::vector<T> out;
  readTable<T>(name, cols,
               [&out](T && rec){out.emplace_back(std::forward<T>(rec));},
               filters, sorting, iStart, count);
  return out;
}
//-----------------------------------------------------------------------------------
template <typename T>
std::vector<T> SQLiteStorageViewEngine::readTable(
      std::vector<SQLiteStorageViewEngine::Filter> const& filters,
      std::optional<Sorting> const& sorting,
      std::optional<int> iStart,
      std::optional<int> count)const
{
  return readTable<T>(
        SQLiteTable<T>(), SQLiteColumns<T>(),
        filters, sorting, iStart, count);
}
//-----------------------------------------------------------------------------------
template <typename T>
SqlInserter<T> SQLiteStorageModificationEngine::buildInserter(SqlInserterMode mode)
{
  return {m_db, mode};
}

//-----------------------------------------------------------------------------------
template<typename SelV, typename TgtV>
SQLiteStorageModificationEngine::Result
SQLiteStorageModificationEngine::updateSelectedValues(
    std::string const&table,
    SqlCol<SelV> const&selectCol,
    std::vector<SelV>const& selectValues,
    SqlCol<TgtV> const&targetCol,
    TgtV const& newValue)
{
  if(selectValues.empty())
  {
    return {};
  }
  std::ostringstream oss;
  oss<<"UPDATE "<<table
     <<" SET "<<targetCol.name<<" = "<<newValue
     << " WHERE "<<selectCol.name<<" IN "
     << "("
     << selectValues.front();
  for(auto it=selectValues.begin()+1;it!=selectValues.end();++it)
  {
    oss << ", "<< *it;
  }
  oss << ")";
  std::string const query=oss.str();
  char* errorMessage;
  int rc = sqlite3_exec(m_db, query.c_str(), NULL, NULL, &errorMessage);
  return (rc==SQLITE_OK)?Result{}:Result{{errorMessage}};
}

//-----------------------------------------------------------------------------------
template<typename SelV>
SQLiteStorageModificationEngine::Result
SQLiteStorageModificationEngine::deleteRecords(
      std::string const&table,
      SqlCol<SelV> const&selectCol,
      std::vector<SelV>const& selectValues)
{
  if(selectValues.empty())
  {
    return {};
  }
  std::ostringstream oss;
  oss<<"DELETE FROM "<<table
     << " WHERE "<<selectCol.name<<" IN "
     << "("
     << selectValues.front();
  for(auto it=selectValues.begin()+1;it!=selectValues.end();++it)
  {
    oss << ", "<< *it;
  }
  oss << ")";
  return execSql(oss);
}
//-----------------------------------------------------------------------------------
template <typename T>
SQLiteStorageCreationEngine::Result
SQLiteStorageCreationEngine::createTable(std::string const& additional)
{
  std::ostringstream oss;
  oss << "CREATE TABLE " << SQLiteTable<T>()<< " (";
  bool hasprevious = false;
  forEachColumn<T>([&](auto sqlCol)
  {
    if(hasprevious)
    {
      oss <<" ,";
    }
    oss << sqlCol.info.name
        << " " << colType(sqlCol);
    switch (sqlCol.info.kind)
    {
    case SqlColKind::AUTO_PRIMARY_KEY:
    {
      oss << " PRIMARY KEY AUTOINCREMENT";
      break;
    }
    case SqlColKind::UNIQUE:
    {
      oss << " UNIQUE";
      break;
    }
    case SqlColKind::REGULAR:
    {
      break;
    }
    }
    hasprevious = true;
  });

  if(!additional.empty())
  {
    oss << ", " << additional;
  }

  oss <<" )";

  sqlite3_stmt*        stmt;
  std::string query=oss.str();
  sqlite3_prepare(m_db, query.data(), query.size(), &stmt, NULL);

  auto rc = sqlite3_step (stmt);
  sqlite3_finalize(stmt);
  return (rc==SQLITE_DONE)?Result{}:Result{{sqlite3_errmsg(m_db)}};
}
