#include "pserver.hpp"
#include "service.hpp"
#include "types.hpp"
#include <unistd.h>


class pserver::engine: public service::engine_type {};

void pserver::initialize(int rd, int wd, std::shared_ptr<icalc> target)
{
  _rd = rd;
  _wd = wd;
  _impl = std::make_shared<engine>();
  _target = target;
}

void pserver::run()
{
  engine::options_type opt;
  opt.target = _target;
  _impl->start(opt, create_id() );

  auto io_id = create_id();  
  char buff[1024];
  for (;;)
  {
    int s = ::read( _rd, buff, 1024 );
    LOG_READ(buff, buff + s)

    auto d = std::make_unique<wjrpc::data_type>( buff, buff + s );

    _impl->perform_io( std::move(d), io_id, [this]( wjrpc::data_ptr d )
    {
      LOG_WRITE(d->begin(), d->end() )
      ::write( this->_wd, d->data(), d->size());
    });
  }
}
