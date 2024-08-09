//
// async_tcp_client.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2024 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "async_tcp_client.h"

using boost::asio::steady_timer;
using boost::asio::ip::tcp;
using std::placeholders::_1;
using std::placeholders::_2;

//Defined in control_tcp.cpp
void handle_tcp_message(const std::string &message, client &conn, bool *pbComms);

//
// This class manages socket timeouts by applying the concept of a deadline.
// Some asynchronous operations are given deadlines by which they must complete.
// Deadlines are enforced by an "actor" that persists for the lifetime of the
// client object:
//
//  +----------------+
//  |                |
//  | check_deadline |<---+
//  |                |    |
//  +----------------+    | async_wait()
//              |         |
//              +---------+
//
// If the deadline actor determines that the deadline has expired, the socket
// is closed and any outstanding operations are consequently cancelled.
//
// Connection establishment involves trying each endpoint in turn until a
// connection is successful, or the available endpoints are exhausted. If the
// deadline actor closes the socket, the connect actor is woken up and moves to
// the next endpoint.
//
//  +---------------+
//  |               |
//  | start_connect |<---+
//  |               |    |
//  +---------------+    |
//           |           |
//  async_-  |    +----------------+
// connect() |    |                |
//           +--->| handle_connect |
//                |                |
//                +----------------+
//                          :
// Once a connection is     :
// made, the connect        :
// actor forks in two -     :
//                          :
// an actor for reading     :       and an actor for
// inbound messages:        :       sending heartbeats:
//                          :
//  +------------+          :          +-------------+
//  |            |<- - - - -+- - - - ->|             |
//  | start_read |                     | start_write |<---+
//  |            |<---+                |             |    |
//  +------------+    |                +-------------+    | async_wait()
//          |         |                        |          |
//  async_- |    +-------------+       async_- |    +--------------+
//   read_- |    |             |       write() |    |              |
//  until() +--->| handle_read |               +--->| handle_write |
//               |             |                    |              |
//               +-------------+                    +--------------+
//
// The input actor reads messages from the socket, where messages are delimited
// by the newline character. The deadline for a complete message is 30 seconds.
//
// The heartbeat actor sends a heartbeat (a message that consists of a single
// newline character) every 10 seconds. In this example, no deadline is applied
// to message sending.
//
  client::client(boost::asio::io_context& io_context, bool *_pbComms)
    : socket_(io_context),
      deadline_(io_context),
      write_deadline_(io_context),
      pbComms(_pbComms)
  {
  }

  // Called by the user of the client class to initiate the connection process.
  // The endpoints will have been obtained using a tcp::resolver.
  void client::start(tcp::resolver::results_type endpoints)
  {
    // Start the connect actor.
    endpoints_ = endpoints;
    start_connect(endpoints_.begin());

    // Start the deadline actor. You will note that we're not setting any
    // particular deadline here. Instead, the connect and input actors will
    // update the deadline prior to each asynchronous operation.
    deadline_.async_wait(std::bind(&client::check_deadline, this));

    write_deadline_.async_wait(std::bind(&client::check_write_deadline, this));
  }

  void client::queue_write_line(const std::string& line)
  {
    std::lock_guard<std::mutex> lg(mutex_queue_tx);
    queue_tx.push(line);
    boost::asio::post(socket_.get_executor(), std::bind(&client::check_write_queue,this));
  }

  void client::check_write_queue()
  {
    if(tx_running)
      return;
    std::string txData;
    {
      std::lock_guard<std::mutex> lg(mutex_queue_tx);
      if(queue_tx.empty())
          return;
      txData = queue_tx.front();
      queue_tx.pop();

    }
    tx_running = true;
    start_write(txData);
  }

  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
  void client::stop()
  {
    stopped_ = true;
    boost::system::error_code ignored_error;
    socket_.close(ignored_error);
    deadline_.cancel();
  }

  void client::start_connect(tcp::resolver::results_type::iterator endpoint_iter)
  {
    if (endpoint_iter != endpoints_.end())
    {
      std::cout << "Trying " << endpoint_iter->endpoint() << "...\n";

      // Set a deadline for the connect operation.
      deadline_.expires_after(std::chrono::seconds(10));

      // Start the asynchronous connect operation.
      socket_.async_connect(endpoint_iter->endpoint(),
          std::bind(&client::handle_connect,
            this, _1, endpoint_iter));
    }
    else
    {
      // There are no more endpoints to try. Shut down the client.
      stop();
    }
  }

  void client::handle_connect(const boost::system::error_code& error,
      tcp::resolver::results_type::iterator endpoint_iter)
  {
    if (stopped_)
      return;

    // The async_connect() function automatically opens the socket at the start
    // of the asynchronous operation. If the socket is closed at this time then
    // the timeout handler must have run first.
    if (!socket_.is_open())
    {
      std::cout << "Connect timed out\n";

      // Try the next available endpoint.
      start_connect(++endpoint_iter);
    }

    // Check if the connect operation failed before the deadline expired.
    else if (error)
    {
      std::cout << "Connect error: " << error.message() << "\n";

      // We need to close the socket used in the previous connection attempt
      // before starting a new one.
      socket_.close();

      // Try the next available endpoint.
      start_connect(++endpoint_iter);
    }

    // Otherwise we have successfully established a connection.
    else
    {
      std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";

      // Start the input actor.
      start_read();

    }
  }

  void client::start_read()
  {
    // Set a deadline for the read operation.
    deadline_.expires_after(std::chrono::seconds(5));

    // Start an asynchronous operation to read a newline-delimited message.
    boost::asio::async_read_until(socket_,
        boost::asio::dynamic_buffer(input_buffer_), '\r',
        std::bind(&client::handle_read, this, _1, _2));
  }

  void client::handle_read(const boost::system::error_code& error, std::size_t n)
  {
    if (stopped_)
      return;

    if (!error)
    {
      // Extract the newline-delimited message from the buffer.
      std::string line(input_buffer_.substr(0, n - 1));
      input_buffer_.erase(0, n);

      std::cout << "Received: " << line << "\n";
      handle_tcp_message(line, *this, pbComms);

      start_read();
    }
    else
    {
      std::cout << "Error on receive: " << error.message() << "\n";

      stop();
    }
  }

  void client::start_write(const std::string & line)
  {
    if (stopped_)
      return;

    // Write operations should take significantly less than 5 seconds...
    write_deadline_.expires_after(std::chrono::seconds(5));

    std::cout << "Writing: " << line << "\n";
    // Start an asynchronous operation to send a message.
    boost::asio::async_write(socket_, boost::asio::buffer(line + "\r"),
        std::bind(&client::handle_write, this, _1));
  }

  void client::handle_write(const boost::system::error_code& error)
  {
    tx_running = false;
    if (stopped_)
      return;

    if (!error)
    {
      // Check if there's more data in the queue, and send it if there is...
      check_write_queue();
    }
    else
    {
      std::cout << "Error on transmit: " << error.message() << "\n";

      stop();
    }
  }

  void client::check_deadline()
  {
    if (stopped_)
      return;

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (deadline_.expiry() <= steady_timer::clock_type::now())
    {
      // The deadline has passed. The socket is closed so that any outstanding
      // asynchronous operations are cancelled.
      socket_.close();

      // There is no longer an active deadline. The expiry is set to the
      // maximgm time point so that the actor takes no action until a new
      // deadline is set.
      deadline_.expires_at(steady_timer::time_point::max());
    }

    // Put the actor back to sleep.
    deadline_.async_wait(std::bind(&client::check_deadline, this));
  }

  void client::check_write_deadline()
  {
    if (stopped_)
      return;
    
    if (!tx_running)
    {
      write_deadline_.expires_at(steady_timer::time_point::max());
      return;
    }

    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (write_deadline_.expiry() <= steady_timer::clock_type::now())
    {
      // The deadline has passed. The socket is closed so that any outstanding
      // asynchronous operations are cancelled.
      socket_.close();

      // There is no longer an active deadline. The expiry is set to the
      // maximgm time point so that the actor takes no action until a new
      // deadline is set.
      write_deadline_.expires_at(steady_timer::time_point::max());
    }

    // Put the actor back to sleep.
    write_deadline_.async_wait(std::bind(&client::check_deadline, this));
  }


#if 0
int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: client <host> <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    tcp::resolver r(io_context);
    client c(io_context);

    c.start(r.resolve(argv[1], argv[2]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
#endif
