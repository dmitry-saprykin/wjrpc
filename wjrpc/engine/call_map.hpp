//
// Author: Vladimir Migashko <migashko@gmail.com>, (C) 2011-2016
//
// Copyright: See COPYING file that comes with this distribution
//

#pragma once

#include <wjrpc/incoming/incoming_holder.hpp>

#include <queue>
#include <mutex>
#include <chrono>


namespace wjrpc{

class call_map
{
public:
  typedef std::function<void(incoming_holder holder)> result_handler_t;
  typedef std::map<call_id_t, result_handler_t> result_map;
  typedef std::mutex mutex_type;

  typedef std::chrono::time_point<std::chrono::system_clock> time_point_t;
  typedef std::pair<time_point_t, call_id_t> time_pair;
  typedef std::priority_queue< time_pair > time_queue;
  typedef std::deque<call_id_t> call_list;
  
  void set_lifetime(time_t lifetime_ms, bool everytime) 
  {
    std::lock_guard<mutex_type> lk(_mutex);
    _everytime = everytime;
    _lifetime_ms = lifetime_ms;
    if ( _lifetime_ms == 0 )
      time_queue().swap(_time_queue);
  }
  

  void set(call_id_t call_id, result_handler_t result)
  {
    if ( _everytime ) this->remove_outdated();
    std::lock_guard<mutex_type> lk(_mutex);
    _result_map[call_id] = result;
    if ( _lifetime_ms != 0 )
    {
      _time_queue.emplace( std::chrono::system_clock::now() + std::chrono::milliseconds(_lifetime_ms), call_id);
      //_time_queue.emplace( this->now_ms() + _lifetime_ms, call_id);
    }
  }
  
  result_handler_t detach(call_id_t call_id)
  {
    //if ( _everytime ) this->remove_outdated();
    std::lock_guard<mutex_type> lk(_mutex);
    result_handler_t result = nullptr;
    auto itr = _result_map.find(call_id);
    if ( itr!=_result_map.end() )
    {
      result = std::move( itr->second );
      _result_map.erase(itr);
    }
    return std::move(result);
  }
  
  void clear()
  {
    result_map tmp_map;
    {
      std::lock_guard<mutex_type> lk(_mutex);
      _result_map.swap(tmp_map);
    }
    
    for (auto& tmp : tmp_map)
    {
      if (tmp.second != nullptr ) 
      {
        tmp.second( incoming_holder( data_ptr() ) );
      }
    }
  }
  
  size_t remove_outdated()
  {
    size_t count = 0;
    auto calls = this->get_call_list();
    for ( auto call_id : calls ) 
    {
      if ( auto handler = this->detach(call_id) )
      {
        handler( incoming_holder( data_ptr() ) );
        ++count;
      }
    }
    return count;
  }

   std::pair<size_t, size_t> sizes() const
   {
     std::lock_guard<mutex_type> lk(_mutex);
     return std::make_pair( _result_map.size(), _time_queue.size() );
   }

private:

  call_list get_call_list()
  {
    std::lock_guard<mutex_type> lk(_mutex);
    call_list calls;
    //auto now = this->now_ms();
    auto now = std::chrono::system_clock::now();
    while ( !_time_queue.empty() && _time_queue.top().first < now )
    {
      calls.push_back( std::move(_time_queue.top().second) );
      _time_queue.pop();
    }
    return std::move( calls );
  }

  /*
  time_t now_ms()
  {
    return std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() ).count();
    //return std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
  }
  */

private:
  bool _everytime = true;
  time_t _lifetime_ms = 1000;
  time_queue _time_queue;
  result_map _result_map;
  mutable mutex_type _mutex;
};

} // wfc

