#pragma once

#include <iow/ip/tcp/acceptor/options.hpp>
#include <iow/ip/tcp/connection/options_json.hpp>
#include <iow/ip/tcp/connection/connection.hpp>
#include <iow/io/acceptor/options_json.hpp>
#include <wjson/json.hpp>

namespace iow{ namespace ip{ namespace tcp{ namespace acceptor{

template<
  typename AcceptorOptions = ::iow::ip::tcp::acceptor::options<>,
  typename ConnectionOptionsJson = ::iow::ip::tcp::connection::options_json
>
using options_json = ::iow::io::acceptor::options_json<AcceptorOptions, ConnectionOptionsJson >;
/*
struct options_json
{
  JSON_NAME(port)
  JSON_NAME(backlog)
  
  typedef json::object<
    options,
    json::member_list<
      json::base< 
        ::iow::io::acceptor::options_json< 
          ::iow::ip::tcp::acceptor::connection, 
          ::iow::ip::tcp::connection::options_json 
        > 
      >
    >
  > type;
  typedef type::target target;
  typedef type::serializer serializer;
  typedef type::member_list member_list;
};
*/

}}}}

