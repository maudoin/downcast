#pragma once

#include <mutex>

template <typename T>
class MutexedData
{
  public:

  template <typename ...Args>
  MutexedData(Args&&...args) : m_data(std::forward<Args>(args)...){}

  class Locked
  {
  public:
    Locked(MutexedData& parent)
      : m_data(parent.m_data)
      , m_lock(parent.m_mutex){}
    T* operator->(){return &m_data;}
    T& operator*(){return m_data;}
  private:
    std::lock_guard<std::recursive_mutex> m_lock;
    T& m_data;
  };
  class LockedConst
  {
  public:
    LockedConst(MutexedData const& parent)
      : m_data(parent.m_data)
      , m_lock(parent.m_mutex){}
    T const* operator->()const{return &m_data;}
    T const& operator*()const{return m_data;}
  private:
    std::lock_guard<std::recursive_mutex> m_lock;
    T const& m_data;
  };
  Locked lock(){return {*this};}
  LockedConst lock()const{return {*this};}

private:
  T m_data;
  mutable std::recursive_mutex m_mutex;
};
