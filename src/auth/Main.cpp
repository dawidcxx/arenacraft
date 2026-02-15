#include <array>
#include <cstdint>
#include <exception>
#include <sstream>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/run.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/cobalt/this_coro.hpp>

#include "Include/Logging.hpp"


namespace asio = boost::asio;
using tcp      = asio::ip::tcp;

boost::cobalt::task<void> handle_client(tcp::socket socket)
{
  try
  {
    static constexpr std::string_view kGreeting = "Welcome to the auth TCP service on port 3724.\\n";

    co_await asio::async_write(socket, asio::buffer(kGreeting), boost::cobalt::use_op);

    std::array<char, 1024> buffer{};
    for (;;)
    {
      const std::size_t bytes = co_await socket.async_read_some(asio::buffer(buffer), boost::cobalt::use_op);

      if (bytes == 0)
      {
        break;
      }

      co_await asio::async_write(socket, asio::buffer(buffer.data(), bytes), boost::cobalt::use_op);
    }
  }
  catch (const std::exception& ex)
  {
    logging::write(logging::Level::error, std::string("Client session ended with error: ") + ex.what());
  }
}

boost::cobalt::task<void> run_server(const std::uint16_t port)
{
  auto executor = co_await boost::cobalt::this_coro::executor;

  tcp::acceptor acceptor(executor, tcp::endpoint(tcp::v4(), port));
  acceptor.set_option(tcp::acceptor::reuse_address(true));

  {
    std::ostringstream message;
    message << "Auth TCP service listening on 0.0.0.0:" << port;
    logging::write(logging::Level::info, message.str());
  }

  for (;;)
  {
    tcp::socket socket = co_await acceptor.async_accept(boost::cobalt::use_op);

    boost::system::error_code ec;
    const auto                remote = socket.remote_endpoint(ec);
    if (!ec)
    {
      std::ostringstream message;
      message << "Accepted connection from " << remote;
      logging::write(logging::Level::info, message.str());
    }

    boost::cobalt::spawn(executor, handle_client(std::move(socket)),
                         [](std::exception_ptr ep)
                         {
                           if (!ep)
                           {
                             return;
                           }

                           try
                           {
                             std::rethrow_exception(ep);
                           }
                           catch (const std::exception& ex)
                           {
                             logging::write(logging::Level::error, std::string("Detached client task error: ") + ex.what());
                           }
                         });
  }
}

int main()
{
  try
  {
    boost::cobalt::run(run_server(3724));
  }
  catch (const std::exception& ex)
  {
    logging::write(logging::Level::error, std::string("Fatal server error: ") + ex.what());
    return 1;
  }

  return 0;
}
