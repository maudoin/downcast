#pragma once

#include <shared_mutex>

template <typename T>
class SharedMutexedData
{
  public:

  template <typename ...Args>
  SharedMutexedData(Args&&...args) : m_data(std::forward<Args>(args)...){}

  class WriteLocked
  {
  public:
    WriteLocked(SharedMutexedData& parent)
      : m_parent(parent)
      //, m_lock(parent.m_mutex)
    {
      m_parent.m_mutex.lock();
    }
    ~WriteLocked(){m_parent.m_mutex.unlock();}
    T* operator->(){return &m_parent.m_data;}
    T& operator*(){return m_parent.m_data;}
    private:
    //std::unique_lock<std::shared_mutex> m_lock;
    SharedMutexedData<T>& m_parent;
  };
  class ReadLocked
  {
  public:
    ReadLocked(SharedMutexedData const& parent)
      : m_data(parent.m_data)
      , m_lock(parent.m_mutex){}
    T const* operator->()const{return &m_data;}
    T const& operator*()const{return m_data;}
  private:
    std::shared_lock<std::shared_mutex> m_lock;
    T const& m_data;
  };
  WriteLocked writeLock(){return {*this};}
  ReadLocked readLock()const{return {*this};}

private:
  T m_data;
  mutable std::shared_mutex m_mutex;
};
