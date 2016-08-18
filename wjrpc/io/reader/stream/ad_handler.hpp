#pragma once

#include <iow/io/reader/stream/tags.hpp>
#include <memory>

namespace iow{ namespace io{ namespace reader{ namespace stream{
  
struct ad_handler
{
  template<typename T>
  void operator()(T& t)
  {
    auto& buf = t.get_aspect().template get<_read_buffer_>();
    while (auto d = buf.detach() )
    {
      if (d->empty() )
      {
        std::cout << "Кривая ошибка" << std::endl;
        continue;
        // Кривая ошибка
        // t.get_aspect().template get<_aaaa_>()(t, "");
      }
      t.get_aspect().template get<_incoming_>()(t, std::move(d) );
    }
  }
};


}}}}
