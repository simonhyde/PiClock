#ifndef __ASYNC_TCP_CLIENT_H_INCLUDED
#define __ASYNC_TCP_CLIENT_H_INCLUDED

//
// async_tcp_client.h
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <functional>
#include <iostream>
#include <string>


class client
{
public:
  client(boost::asio::io_context& io_context);
  void start(tcp::resolver::results_type endpoints);
  void stop();

private:
  void start_connect(tcp::resolver::results_type::iterator endpoint_iter);

  void handle_connect(const boost::system::error_code& error,
      tcp::resolver::results_type::iterator endpoint_iter);

  void start_read();

  void handle_read(const boost::system::error_code& error, std::size_t n);

  void start_write();

  void handle_write(const boost::system::error_code& error);

  void check_deadline()

private:
  bool stopped_ = false;
  boost::asio::ip::tcp::resolver::results_type endpoints_;
  boost::asio::ip::tcp::socket socket_;
  std::string input_buffer_;
  boost::asio::steady_timer deadline_;
  boost::asio::steady_timer heartbeat_timer_;
};
#endif
