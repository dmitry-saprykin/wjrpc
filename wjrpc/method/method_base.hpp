//
// Author: Vladimir Migashko <migashko@gmail.com>, (C) 2011-2016
//
// Copyright: See COPYING file that comes with this distribution
//

#pragma once

#include <wjrpc/method/aspect/tags.hpp>
#include <wjrpc/logger.hpp>
#include <fas/aop.hpp>
#include <memory>

namespace wjrpc{
  
template< 
  typename A = fas::aspect<>, 
  template<typename> class AspectClass = fas::aspect_class 
>
class method_base
  : public AspectClass<A>
{
public:
  typedef method_base<A> self;
  typedef AspectClass<A> super;

  
  // TODO: в _method_types_;
  typedef ::wjrpc::data_type data_type;
  typedef ::wjrpc::data_ptr  data_ptr;

  
  typedef fas::metalist::advice metatype;
  typedef typename super::aspect::template advice_cast<_name_>::type::name_type tag;
  typedef self advice_class;

  advice_class& get_advice() { return *this;};
  const advice_class& get_advice() const { return *this;}

  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////

  const char* name() const
  {
    return this->get_aspect().template get<_name_>()();
  }

  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////

  template<typename T, typename OutgoingHandler>
  void operator()(
    T& t, 
    incoming_holder holder, 
    OutgoingHandler outgoing_handler
  ) 
  {
    this->get_aspect().template get<_invoke_>()(
      t, 
      *this, 
      std::move(holder), 
      std::move(outgoing_handler) 
    );
  }
  
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  
  template<typename T, typename ParamsPtr, typename Callback>
  void call(T& t, ParamsPtr params, Callback callback) const
  {

    this->get_aspect().template get<_call_>()( 
      t, 
      *this, 
      std::move(params), 
      std::move(callback) 
    );
  }

  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  
  template<typename T, typename ParamsJson>
  static inline auto serialize_request(
    const char* name,
    std::unique_ptr<typename ParamsJson::target> req, 
    typename T::call_id_t id
  ) -> typename T::data_ptr
  {
    return super::aspect::template advice_cast<_request_serializer_>
                ::type::template serialize<T, ParamsJson>( 
                    name, 
                    std::move(req), 
                    id
                );
  }

  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////

  template<typename T, typename ParamsJson>
  static inline auto serialize_notify(
    const char* name, 
    std::unique_ptr<typename ParamsJson::target> req
  ) -> typename T::data_ptr
  {
    return super::aspect::template advice_cast<_notify_serializer_>::type
                ::template serialize<T, ParamsJson>( name, std::move(req) );
  }

  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////

  template<typename T, typename ResultJson, typename ErrorJson>
  static inline void process_response(
    incoming_holder holder, 
    std::function< void (
      std::unique_ptr<typename ResultJson::target>, 
      std::unique_ptr<typename ErrorJson::target>
    )> callback
  ) 
  {
    return super::aspect::template advice_cast<_process_response_>::type
            ::template process<T, ResultJson, ErrorJson>( 
                std::move(holder), 
                std::move(callback) 
              );

  }
  
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////

  
  template<typename T, typename ResultJson, typename OutgoingHandler>
  static inline void send_result(
    incoming_holder holder, 
    std::unique_ptr<typename ResultJson::target> result, 
    OutgoingHandler outgoing_handler
  )
  {
    return super::aspect::template advice_cast<_send_result_>::type
            ::template send<T, ResultJson>( 
                std::move(holder), 
                std::move(result), 
                std::move(outgoing_handler) 
            );

  }
  
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////
  /// ///////////////////////////////////////////////////


  template<typename T, typename ErrorJson, typename OutgoingHandler>
  static inline void send_error(
    incoming_holder holder, 
    std::unique_ptr<typename ErrorJson::target> err, 
    OutgoingHandler outgoing_handler
  )
  {
    return super::aspect::template advice_cast<_send_error_>::type
            ::template send<T, ErrorJson>( 
                std::move(holder), 
                std::move(err), 
                std::move(outgoing_handler) 
            );

  }
};

}
