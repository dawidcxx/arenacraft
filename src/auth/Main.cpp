#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/run.hpp>
#include <boost/cobalt/spawn.hpp>
#include <boost/cobalt/task.hpp>
#include <boost/cobalt/this_coro.hpp>

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
    std::cerr << "Client session ended with error: " << ex.what() << std::endl;
  }
}

boost::cobalt::task<void> run_server(const std::uint16_t port)
{
  auto executor = co_await boost::cobalt::this_coro::executor;

  tcp::acceptor acceptor(executor, tcp::endpoint(tcp::v4(), port));
  acceptor.set_option(tcp::acceptor::reuse_address(true));

  std::cout << "Auth TCP service listening on 0.0.0.0:" << port << std::endl;

  for (;;)
  {
    tcp::socket socket = co_await acceptor.async_accept(boost::cobalt::use_op);

    boost::system::error_code ec;
    const auto                remote = socket.remote_endpoint(ec);
    if (!ec)
    {
      std::cout << "Accepted connection from " << remote << std::endl;
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
                             std::cerr << "Detached client task error: " << ex.what() << std::endl;
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
    std::cerr << "Fatal server error: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
