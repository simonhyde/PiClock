
#include <string>
#include <thread>
#include <mutex>
#include <openssl/sha.h>
#include "blocking_tcp_client.h"
#include "globals.h"
#include "control_tcp.h"
#include "piclock_messages.h"



int handle_tcp_message(const std::string &message, client & conn)
{
	std::string cmd = get_arg(message,0);
	if(cmd == "PING")
	{
		conn.write_line("PONG", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		return 2;
	}
	else if(cmd == "CRYPT")
	{
		uint8_t sha_buf[SHA512_DIGEST_LENGTH];
		std::string to_digest = get_arg(message,1,false) + TALLY_SECRET;
		SHA512((const uint8_t *)to_digest.c_str(), to_digest.length(),
			sha_buf);
		char out_buf[SHA512_DIGEST_LENGTH*2 + 1];
		for(int i = 0; i < SHA512_DIGEST_LENGTH; i++)
			sprintf(out_buf + i*2, "%02x", sha_buf[i]);
		std::string to_write = std::string("AUTH:") + std::string(out_buf)
				   + std::string(":") + mac_address;
		conn.write_line(to_write, boost::posix_time::time_duration(0,0,10),'\r');
		return 3;
	}
	else
	{
		auto parsed = ClockMsg_Parse(message);
		if(parsed)
		{
			msgQueue.Add(parsed);
		}
		else
		{
			//Unknwon command, just NACK
			conn.write_line("NACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		}
	}
	conn.write_line("ACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
	return 1;
}

void tcp_thread(std::string remote_host, bool * pbComms)
{
	int retryDelay = 0;
	std::string service = TALLY_SERVICE;
	auto colon_index = remote_host.find(':');
	if(colon_index != std::string::npos)
	{
		service = remote_host.substr(colon_index + 1);
		remote_host = remote_host.substr(0, colon_index);
	}
	while(bRunning)
	{
		try
		{
			*pbComms = false;
			client conn;
			//Allow 30 seconds for connection
			conn.connect(remote_host, service,
				boost::posix_time::time_duration(0,0,30,0));
			while(bRunning)
			{
				//Nothing for 5 seconds should prompt a reconnect
				std::string data = conn.read_line(
				boost::posix_time::time_duration(0,0,5,0),'\r');

				int ret = handle_tcp_message(data, conn);
				if(ret == 0)
					break;
				if(ret != 3) //Login attempt might not be good
				{
					retryDelay = 0;
					*pbComms = true;
				}
			}
		}
		catch(...)
		{}
		if(retryDelay++ > 30)
			retryDelay = 30;
		sleep(retryDelay);
	}
}


void create_tcp_threads()
{
	//Determin MAC address...
	struct ifreq s;
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	strcpy(s.ifr_name, "eth0");
	if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
	{
		char mac_buf[80];
		sprintf(mac_buf,"%02x:%02x:%02x:%02x:%02x:%02x",
				(unsigned char) s.ifr_addr.sa_data[0],
				(unsigned char) s.ifr_addr.sa_data[1],
				(unsigned char) s.ifr_addr.sa_data[2],
				(unsigned char) s.ifr_addr.sa_data[3],
				(unsigned char) s.ifr_addr.sa_data[4],
				(unsigned char) s.ifr_addr.sa_data[5]);
		mac_address = mac_buf;
	}
	else
	{
		mac_address = "UNKNOWN";
	}
	for(unsigned int i = 0; i < tally_hosts.size(); i++)
	{
		std::thread t(tcp_thread, tally_hosts[i], &(bComms[i]));
		t.detach();
	}
}


