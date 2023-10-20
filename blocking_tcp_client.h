#ifndef __PICLOCK_BLOCKING_TCP_CLIENT_H
#define __PICLOCK_BLOCKING_TCP_CLIENT_H

#include <string>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deadline_timer.hpp>


class client
{
public:
  client();
  void connect(const std::string& host, const std::string& service,
      boost::posix_time::time_duration timeout);

  std::string read_line(boost::posix_time::time_duration timeout, char separator = '\n');
 
  void write_line(const std::string& line,
      boost::posix_time::time_duration timeout, char separator = '\n');
  void close();
  ~client();

private:
  void check_deadline();

  boost::asio::io_service io_service_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::deadline_timer deadline_;
  boost::asio::streambuf input_buffer_;
};


#endif