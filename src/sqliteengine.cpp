
#include "storage.h"

#include "sqlite3.h"

#include <assert.h>

//-----------------------------------------------------------------------------------
SQLiteStorageEngineBase::SQLiteStorageEngineBase(std::string const& dbPath)
  :m_dbPath(dbPath)
{
}
//-----------------------------------------------------------------------------------
SQLiteStorageEngineBase::~SQLiteStorageEngineBase()
{
  close();
}
//-----------------------------------------------------------------------------------
void SQLiteStorageEngineBase::close()
{
  if(m_db)
  {
    sqlite3_close_v2(m_db);
    m_db = nullptr;
  }
}
//-----------------------------------------------------------------------------------
SQLiteStorageEngineBase::Result SQLiteStorageEngineBase::open(SQLiteStorageEngineBase::Mode mode)
{
  const int rc = sqlite3_open_v2(m_dbPath.c_str(), &m_db,
                                 mode==Mode::ReadWriteCreate?
                                   SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE:
                                   mode==Mode::ReadWrite?
                                     SQLITE_OPEN_READWRITE:
                                     SQLITE_OPEN_READONLY,
                                 NULL);
  m_opened = (rc==SQLITE_OK);
  return m_opened?Result{}:Result{{sqlite3_errmsg(m_db)}};
}
//-----------------------------------------------------------------------------------
SQLiteStorageEngineBase::operator bool() const
{
  return m_opened;
}
//-----------------------------------------------------------------------------------

SQLiteStorageViewEngine::SQLiteStorageViewEngine(std::string const& dbPath,
                                                 Mode mode)
  :SQLiteStorageEngineBase(dbPath)
{
  open(mode);
}
//-----------------------------------------------------------------------------------
int SQLiteStorageViewEngine::countTable(std::string const& table, std::vector<Filter> const& filters)
{

  sqlite3_stmt*        stmt;
  std::ostringstream oss;
  oss << "SELECT COUNT(*) FROM " << table;
  if(filters.size()>0)
  {
    oss << " WHERE " << filters.front().col << " = "<< filters.front().value;
    std::for_each(filters.begin()+1, filters.end(),
                  [&](Filter const& f)
    {
      oss << " AND "<< f.col << " = "<< f.value;
    });
  }
  std::string const query=oss.str();
  sqlite3_prepare(m_db, query.data(), query.size(), &stmt, NULL);
  auto stepState = sqlite3_step (stmt);
  assert(stepState==SQLITE_ROW);
  int count =  sqlite3_column_int(stmt, 0);
  assert(sqlite3_step (stmt)==SQLITE_DONE);
  sqlite3_finalize(stmt);
  return count;
}

//-----------------------------------------------------------------------------------
SQLiteStorageModificationEngine::
SQLiteStorageModificationEngine(std::string const& dbPath,
                                Mode mode)
  : SQLiteStorageViewEngine(dbPath, mode)
{
}

//-----------------------------------------------------------------------------------
SQLiteStorageModificationEngine::
SQLiteStorageModificationEngine(std::string const& dbPath)
  : SQLiteStorageModificationEngine(dbPath, Mode::ReadWrite)
{
}

//-----------------------------------------------------------------------------------
SQLiteStorageCreationEngine::SQLiteStorageCreationEngine(std::string const& dbPath)
  : SQLiteStorageModificationEngine(dbPath, Mode::ReadWriteCreate)
{
}
//-----------------------------------------------------------------------------------
std::string join( std::vector<std::string> const& fields,
                  std::string const& delimiter)
{
  return std::accumulate(std::next(fields.begin()), fields.end(),
                         fields.front(),[&delimiter](auto&& a, auto&& b)
  {
    return a + delimiter + b;
  });
}
