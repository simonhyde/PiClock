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
#include <queue>

using boost::asio::ip::tcp;

class client
{
public:
  client(bool *pbComms);
  void start(tcp::resolver::results_type endpoints);
  void stop();
  void queue_write_line(const std::string& line);

  //bodge
  int last_gpi_value = 0x1FFFF;

  boost::asio::io_context & get_io_context();

private:
  void start_connect(tcp::resolver::results_type::iterator endpoints_iter);

  void handle_connect(const boost::system::error_code& error,
      tcp::resolver::results_type::iterator endpoint_iter);

  void start_read();

  void handle_read(const boost::system::error_code& error, std::size_t n);

  void start_write(const std::string& line);

  void handle_write(const boost::system::error_code& error);

  void check_write_queue();

  void check_deadline();

  void check_write_deadline();

private:
  boost::asio::io_context io_context_;
  bool stopped_ = false;
  tcp::resolver::results_type endpoints_;
  tcp::socket socket_;
  std::string input_buffer_;
  boost::asio::steady_timer deadline_;
  boost::asio::steady_timer write_deadline_;
  std::queue<std::string> queue_tx;
  std::mutex mutex_queue_tx;
  bool tx_running = false;
  bool * pbComms;
};
#endif
