#include "Server.hpp"
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <vector>

namespace rtmp_network {

server::server(const std::string& address, const std::string& port,
               std::size_t thread_pool_size,
               RequestHandlerFactory_ptr handler_factory,
               RequestParserFactory_ptr parser_factory)
    : thread_pool_size_(thread_pool_size),
      signals_(io_service_),
      acceptor_(io_service_),
      new_connection_(),
      new_connection_id_(0),
      handler_factory_(handler_factory),
      parser_factory_(parser_factory){
  // Register to handle the signals that indicate when the server should exit.
  // It is safe to register for the same signal multiple times in a program,
  // provided all registration for the specified signal is made through Asio.
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals_.async_wait(boost::bind(&server::handle_stop, this));

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  boost::asio::ip::tcp::resolver resolver(io_service_);
  boost::asio::ip::tcp::resolver::query query(address, port);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();

  start_accept();
}

void server::run() {
  std::vector<boost::shared_ptr<boost::thread> > threads;
  for (std::size_t i = 0; i < thread_pool_size_; ++i) {
    boost::shared_ptr<boost::thread> thread(
        new boost::thread(
            boost::bind(&boost::asio::io_service::run, &io_service_)));
    threads.push_back(thread);
  }

  for (std::size_t i = 0; i < threads.size(); ++i)
    threads[i]->join();
}

void server::start_accept() {
  new_connection_.reset(
      new Connection(io_service_, handler_factory_, parser_factory_,
                     new_connection_id_++));

  acceptor_.async_accept(
       new_connection_->socket(),
       [this](const boost::system::error_code& e) {
         this->handle_accept(e);
       });
}

void server::handle_accept(const boost::system::error_code& e) {
  if (!e) {
    new_connection_->start();
  }

  start_accept();
}

void server::handle_stop() {
  io_service_.stop();
}

}  // namespace rtmp_network
