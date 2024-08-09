
#include <string>
#include <thread>
#include <mutex>
#include <openssl/sha.h>
#include "async_tcp_client.h"
#include "globals.h"
#include "control_tcp.h"
#include "piclock_messages.h"


static std::shared_ptr<std::vector<std::atomic<std::shared_ptr<client>>>> conns;
static volatile int lastGpioValue = 0x1FFFF;

void handle_tcp_message(const std::string &message, client & conn, bool * pbComms)
{
	std::string cmd = get_arg(message,0);
	if(cmd == "PING")
	{
		conn.queue_write_line("PONG");//2 seconds should be plenty of time to transmit our reply...
		*pbComms = true;
                return;
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
		conn.queue_write_line(to_write);
                //In theory at this point we're authenticated (or it all failed and we're about to be booted out)
                // so we can prompt the system to add the current gpio state to the tx queue for this connection
                conn.last_gpi_value = 0x1FFFF;
                lastGpioValue = 0x1FFFF;
                return;
	}
	else
	{
		auto parsed = ClockMsg_Parse(message);
		if(parsed)
		{
			msgQueue.Add(parsed);
                        conn.queue_write_line("ACK");
		}
		else
		{
			//Unknwon command, just NACK
			conn.queue_write_line("NACK");
		}
	}
        *pbComms = true;
}

void update_tcp_gpis(uint16_t values)
{
    if(!conns)
        return;
    int newval = values;
    //Refuse to update if identical
    if(newval == lastGpioValue)
        return;
    lastGpioValue = newval;
    if(newval > 0xFFFF)
        return;
    std::string cmdStr = std::string("GPI:") + std::to_string(newval);
    for(std::shared_ptr<client> conn: *conns)
    {
        if(!conn)
            return;
        if(conn->last_gpi_value != newval)
        {
            conn->last_gpi_value = newval;
            conn->queue_write_line(cmdStr);
        }
    }
}

static void tcp_thread(std::string remote_host, bool * pbComms, int conn_index)
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
                        boost::asio::io_context io_context;
                        tcp::resolver reslv(io_context);
                        std::shared_ptr<client> conn = std::make_shared<client>(io_context, pbComms);
                        (*conns)[conn_index].store(conn);
                        sleep(retryDelay);
			conn->start(reslv.resolve(remote_host, service));
                        io_context.run();
                        //Retry quickly if we previously succeeded
                        if(*pbComms)
                            retryDelay = 0;
		}
		catch(...)
		{}
		if(retryDelay < 15)
                {
                    int maxIncr = std::max(2, retryDelay + 1);
                    retryDelay += std::rand() / (RAND_MAX/maxIncr);
                }
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
        conns = std::make_shared<std::vector<std::atomic<std::shared_ptr<client>>>>(tally_hosts.size());
	for(unsigned int i = 0; i < tally_hosts.size(); i++)
	{
		std::thread t(tcp_thread, tally_hosts[i], &(bComms[i]), i);
		t.detach();
	}
}


