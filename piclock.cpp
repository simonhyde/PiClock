// OpenVG Clock
// Simon Hyde <simon.hyde@bbc.co.uk>
#include <thread>
#include <mutex>
#include <string>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <netdb.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <openssl/sha.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include "ntpstat/ntpstat.h"
//Bit of a bodge, but generating a header file for this example would be a pain
#include "blocking_tcp_client.cpp"
//piface digital
#include "pifacedigital.h"

#define FPS 25
#define FRAMES 0
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000

namespace po = boost::program_options;

std::string mac_address;
int bRunning = 1;
std::map<unsigned int,bool> bComms;
int GPI_MODE = 0;
std::string TALLY_SERVICE("6254");
std::string TALLY_SECRET("SharedSecretGoesHere");
std::vector<std::string> tally_hosts;


void * ntp_check_thread(void * arg)
{
	ntpstate_t * data = (ntpstate_t *)arg;
	while(bRunning)
	{
		get_ntp_state(data);
		sleep(1);
	}
	return NULL;
}

class TallyColour
{
public:
	uint32_t R() const
	{
		return (col >> 16)&0xFF;
	}
	uint32_t G() const
	{
		return (col >> 8)&0xFF;
	}
	uint32_t B() const
	{
		return col & 0xFF;
	}
	TallyColour(const std::string &input)
		:col(std::stoul(input, NULL, 16))
	{
	}
	TallyColour()
		:col(0) //Default to black
	{
	}
	bool Equals(const TallyColour & other) const
	{
		return other.col == col;
	}

protected:
	uint32_t col;
};

class TallyState
{
public:
	TallyColour FG;
	TallyColour BG;
	std::string text;
	TallyState()
	{}
	TallyState(const std::string &fg, const std::string &bg, const std::string &_text)
	 :FG(fg),BG(bg),text(_text)
	{}
	TallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text)
	 :FG(fg),BG(bg),text(_text)
	{}
	bool Equals(const TallyState & other) const
	{
		return other.FG.Equals(FG)
			&& other.BG.Equals(BG)
			&& other.text == text;
	}
};

class TallyDisplays
{
public:
	int nRows;
	int nCols;
	int textSize;
	std::string sProfName;
	std::map<int,std::map<int,TallyState> > tallies;
	TallyDisplays()
	 :nRows(0), nCols(0), textSize(-1), sProfName()
	{
	}
};

boost::shared_ptr<TallyDisplays> pTallyDisplays(new TallyDisplays());

std::string get_arg(const std::string & input, int index, bool bTerminated = true)
{
	size_t start_index = 0;
	while(index > 0)
	{
		start_index = input.find(':', start_index) + 1;
		index--;
	}

	if(!bTerminated)
		return input.substr(start_index);

	size_t end = input.find(':',start_index);
	if(end != std::string::npos)
		end -= start_index;
	return input.substr(start_index,end);
}

int get_arg_int(const std::string &input, int index, bool bTerminated = true)
{
	return std::stoi(get_arg(input, index));
}

int handle_tcp_message(const std::string &message, client & conn)
{
	if(message == "PING")
	{
		conn.write_line("PONG", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		return 2;
	}
	std::string cmd = get_arg(message,0);
	if(cmd == "CRYPT")
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
	boost::shared_ptr<TallyDisplays> pOld = pTallyDisplays;
	boost::shared_ptr<TallyDisplays> pTd(new TallyDisplays(*pOld));
	
	bool bChanged = false;
	bool bSizeChanged = false;
	if(cmd == "SETSIZE")
	{
		pTd->nRows = get_arg_int(message,1);
		pTd->nCols = get_arg_int(message,2);
		bChanged = pOld->nRows != pTd->nRows || pOld->nCols != pTd->nCols;
		bSizeChanged = bChanged;
	}
	else if(cmd == "SETTALLY")
	{
		int row = get_arg_int(message,1);
		int col = get_arg_int(message,2);
		pTd->tallies[row][col] = TallyState(get_arg(message,3),
						  get_arg(message,4),
						  get_arg(message,5,false));
		bChanged = !pTd->tallies[row][col].Equals(pOld->tallies[row][col]);
		bSizeChanged = pTd->tallies[row][col].text != pOld->tallies[row][col].text;
	}
	else if(cmd == "SETPROFILE")
	{
		pTd->sProfName = get_arg(message, 1);
		bChanged = pTd->sProfName != pOld->sProfName;
	}
	else
	{
		//Unknwon command, just NACK
		conn.write_line("NACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
		return 1;
	}
	if(bChanged)
	{
		if(bSizeChanged)
			pTd->textSize = -1;
		pTallyDisplays = pTd;
	}
	conn.write_line("ACK", boost::posix_time::time_duration(0,0,2),'\r');//2 seconds should be plenty of time to transmit our reply...
	return 1;
}

void tcp_thread(const std::string remote_host, bool * pbComms)
{
	int retryDelay = 0;
	while(bRunning)
	{
		try
		{
			*pbComms = false;
			client conn;
			//Allow 30 seconds for connection
			conn.connect(remote_host, TALLY_SERVICE,
				boost::posix_time::time_duration(0,0,30,0));
			while(bRunning)
			{
				//Nothing for 5 seconds should prompt a reconnect
				std::string data = conn.read_line(
				boost::posix_time::time_duration(0,0,5,0),'\r');

				if(!handle_tcp_message(data, conn))
					break;
				retryDelay = 0;
				*pbComms = true;
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

void read_settings(const std::string & filename,
		   po::variables_map& vm)
{
	po::options_description desc("Options");
	desc.add_options()
		("tally_mode", po::value<int>(&GPI_MODE)->default_value(0), "Tally Mode, 0=disabled, 1=PiFace Digital, 2=TCP/IP")
		("tally_remote_host", po::value<std::vector<std::string>>(&tally_hosts), "Remote tally host, may be specified multiple times for multiple connections")
		("tally_remote_port", po::value<std::string>(&TALLY_SERVICE)->default_value("6254"), "Port (or service) to connect to on (default 6254)")
		("tally_shared_secret", po::value<std::string>(&TALLY_SECRET)->default_value("SharedSecretGoesHere"), "Shared Secret (password) for connecting to tally service")
	;
	
	std::ifstream settings_file(filename.c_str());
	
	vm = po::variables_map();

	po::store(po::parse_config_file(settings_file, desc), vm);
	po::notify(vm);
}

int main(int argc, char *argv[]) {
	int iwidth, iheight;
	fd_set rfds;
	struct timeval timeout;
	VGfloat commsWidth = -1;
	VGfloat commsHeight = -1;
	VGfloat commsTextHeight = -1;
	int i;
	time_t tm_last_comms_good = 0;
	VGfloat hours_x[12];
	VGfloat hours_y[12];
	timeout.tv_usec = timeout.tv_sec = 0;
	std::string configFile = "piclock.cfg";
	if(argc > 1)
		configFile = argv[1];
	po::variables_map vm;
	read_settings(configFile, vm);
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);
	

	init(&iwidth, &iheight);					// Graphics initialization

	struct timeval tval;
	struct tm tm_now;

	VGfloat offset = iheight*.1f;
	VGfloat width = iwidth - offset;
	VGfloat height = iheight - offset;

	VGfloat clock_width = height;
	VGfloat text_width = width - clock_width;

	for(i = 0; i < 12; i++)
	{
		hours_x[i] = cosf(M_PI*i/6.0f) * height*9.0f/20.0f;
		hours_x[i] -= height/30.0f;
		hours_y[i] = sinf(M_PI*i/6.0f) * height*9.0f/20.0f;
	}
	srand(time(NULL));
	int vrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	int hrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	VGfloat h_offset_pos = 0;
	VGfloat v_offset_pos = 0;
	long prev_sec = 0;
	ntpstate_t ntp_state_data;
	init_ntp_state();
	get_ntp_state(&ntp_state_data);
	pthread_t ntp_thread;
	pthread_attr_t ntp_attr;
	pthread_attr_init(&ntp_attr);
	pthread_create(&ntp_thread, &ntp_attr, &ntp_check_thread, &ntp_state_data);
	if(GPI_MODE == 1)
		pifacedigital_open(0);
	else if(GPI_MODE == 2)
		create_tcp_threads();
	while(1)
	{
		std::string profName;
		int i;
		char buf[256];
		gettimeofday(&tval, NULL);
		localtime_r(&tval.tv_sec, &tm_now);
		Start(iwidth, iheight);					// Start the picture
		Background(0, 0, 0);					// Black background
		Fill(255, 255, 255, 1);					// white text
		Stroke(255, 255, 255, 1);				// White strokes
		if(tval.tv_sec != prev_sec)
		{
			prev_sec =  tval.tv_sec;
			h_offset_pos = abs(prev_sec%(hrate*2) - hrate)*offset/hrate;
			v_offset_pos = abs(prev_sec%(vrate*2) - vrate)*offset/vrate;
		}
		//Write out the text
		Translate(h_offset_pos, v_offset_pos);
#if FRAMES
		sprintf(buf,"%02d:%02d:%02d:%02ld",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec,tval.tv_usec *FPS/1000000);
		TextMid(text_width / 2.0f, height * 8.0f/ 10.0f, buf, MonoTypeface, text_width / 10.0f);	// Timecode
#else
		sprintf(buf,"%02d:%02d:%02d",tm_now.tm_hour,tm_now.tm_min,tm_now.tm_sec);
		TextMid(text_width / 2.0f, height * 8.0f/ 10.0f, buf, MonoTypeface, text_width / 7.0f);	// Timecode
#endif
		strftime(buf, sizeof(buf), "%A", &tm_now);
		TextMid(text_width / 2.0f, height * 7.0f/10.0f, buf, SerifTypeface, text_width/15.0f);
		strftime(buf, sizeof(buf), "%d %b %Y", &tm_now);
		TextMid(text_width / 2.0f, height * 6.0f/10.0f, buf, SerifTypeface, text_width/15.0f);
		if(GPI_MODE == 1)
		{
			uint8_t gpis = pifacedigital_read_reg(INPUT,0);
			uint8_t colour_weight = (gpis & 1) ? 100:255;
			Fill(colour_weight,colour_weight*0.55,0,1);
			Roundrect(text_width/100.0f,height*0.5/10.0f,text_width *.98f,height*2.0f/10.0f,width/100.0f, height/100.0f);
			uint8_t fill_weight = (gpis & 1)? 100:255;
			Fill(fill_weight,fill_weight,fill_weight, 1);
			TextMid(text_width /2.0f, height *1.0f/10.0f, "Mic Live", SerifTypeface, text_width/10.0f);
			colour_weight = (gpis & 2) ? 100:255;
			Fill(colour_weight,0,0,1);
			Roundrect(text_width/100.0f,height*3.0/10.0f,text_width *.98f,height*2.0f/10.0f,width/100.0f, height/100.0f);
			fill_weight = (gpis & 2)? 100:255;
			Fill(fill_weight,fill_weight,fill_weight, 1);
			TextMid(text_width /2.0f, height *3.5f/10.0f, "On Air", SerifTypeface, text_width/10.0f);
		}
		else if(GPI_MODE == 2)
		{
			//Copy (reference to) current state, so it won't change whilst we're processing it...
			boost::shared_ptr<TallyDisplays> pTD = pTallyDisplays;
			//Draw comms status
			int pointSize = height/100.0f;
			if(commsWidth < 0)
			{
				commsWidth =
					std::max(TextWidth("Comms OK", SerifTypeface, pointSize),
						 TextWidth("Comms Failed", SerifTypeface, pointSize));
				for(unsigned int window = 0; window < tally_hosts.size(); window++)
				{
					char buf[512];
					buf[511] = '\0';
					snprintf(buf, 511, "Tally Server %d", window + 1);
					commsWidth = std::max(commsWidth, TextWidth(buf, SerifTypeface, pointSize));
				}
				commsWidth *= 1.2f;
				commsTextHeight = TextHeight(SerifTypeface, pointSize);
				commsHeight = 3.2f*commsTextHeight;
			}
			bool bAnyComms = false;
			for(unsigned int window = 0; window < tally_hosts.size(); window++)
			{
				bAnyComms = bAnyComms || bComms[window];
				if(bComms[window])
					Fill(0,100,0,1);
				else
					Fill(190,0,0,1);
				VGfloat base_x = text_width - commsWidth * (VGfloat)(tally_hosts.size()-window);
				VGfloat base_y = 0;
				Rect( base_x, base_y, commsWidth, commsHeight);
				Fill(200,200,200,1);
				char buf[512];
				buf[511] = '\0';
				base_x += commsWidth*.5f;
				snprintf(buf, 511, "Tally Server %d", window + 1);
				TextMid(base_x, base_y + commsTextHeight*1.8f, buf, SerifTypeface, pointSize);
				snprintf(buf, 511, "Comms %s", bComms[window]? "OK" : "Failed");
				TextMid(base_x, base_y + commsTextHeight*0.4f, buf, SerifTypeface, pointSize);
			}
			if(bAnyComms)
			{
				tm_last_comms_good = tval.tv_sec;
			}
			//Stop showing stuff after 5 seconds of comms failed...
			else if(pTD->nRows > 0 && tm_last_comms_good < tval.tv_sec - 5)
			{
				pTallyDisplays = pTD = boost::shared_ptr<TallyDisplays>(new TallyDisplays());
			}
				
			profName = pTD->sProfName;
			if(pTD->nRows > 0 && pTD->nCols > 0)
			{
				//Use 50% of height, 10% up the screen
				VGfloat row_height = height/(2.0f*(VGfloat)pTD->nRows);
				VGfloat col_width = text_width/((VGfloat)pTD->nCols);
				if(pTD->textSize < 0)
				{
					pTD->textSize = 1;
					bool bOverflown = false;
					while(!bOverflown)
					{
						pTD->textSize++;
						if(TextHeight(SerifTypeface, pTD->textSize) > row_height*.9f)
							bOverflown = true;
						for(int row = 0; row < pTD->nRows; row++)
						{
							for(int col = 0; col < pTD->nCols && !bOverflown; col++)
							{
								if(TextWidth(pTD->tallies[row][col].text.c_str(),SerifTypeface, pTD->textSize) > col_width*.9f)
									bOverflown=true;
							}
						}
					}
					pTD->textSize--;
					printf("Optimal Text Size: %d\n",pTD->textSize);
				}
				VGfloat textOffset = -TextHeight(SerifTypeface, pTD->textSize)*.33f;
				//float y_offset = height/10.0f;
				for(int row = 0; row < pTD->nRows; row++)
				{
					VGfloat base_y = ((VGfloat)row)*row_height + std::max(commsHeight*1.1f, height/20.0f);
					for(int col = 0; col < pTD->nCols; col++)
					{
#define curTally (pTD->tallies[row][col])
						VGfloat base_x = ((VGfloat)col)*col_width + text_width/100.0f;
						Fill(curTally.BG.R(),curTally.BG.G(),curTally.BG.B(),1);
						Roundrect(base_x, base_y, col_width*.98f, row_height *.98f,row_height/10.0f, row_height/10.0f);
						Fill(curTally.FG.R(),curTally.FG.G(),curTally.FG.B(), 1);
						TextMid(base_x + col_width/2.0f, textOffset + base_y + row_height/2.0f, curTally.text.c_str(), SerifTypeface, pTD->textSize);
					}
				}
			}
		}
		//Done with tallies, back to common code...
		Fill(127, 127, 127, 1);
		Text(0, 0, "Press Ctrl+C to quit", SerifTypeface, text_width / 150.0f);
		if(GPI_MODE == 2)
		{
			char buf[8192];
			buf[sizeof(buf)-1] = '\0';
			if(profName.empty())
				snprintf(buf, sizeof(buf) -1, "MAC Address %s", mac_address.c_str());
			else
				snprintf(buf, sizeof(buf) -1, "MAC Address %s, %s", mac_address.c_str(), profName.c_str());
			Text(0, TextHeight(SerifTypeface,text_width/150.0f)*1.5f, buf, SerifTypeface, text_width / 100.0f);
		}
		Fill(255,255,255,1);
		const char * sync_text;
		if(ntp_state_data.status == 0)
		{
			Fill(0,100,0,1);
			sync_text = "Synchronised";
		}
		else
		{
			/* flash between red and purple once a second */
			Fill(120,0,(tval.tv_sec %2)*120,1);
			if(ntp_state_data.status == 1)
				sync_text = "Synchronising..";
			else
				sync_text = "Unknown Synch!";
		}
		//Draw a box around NTP status
		Rect(width - clock_width*.2, 0, clock_width*.2, height*.05 );
		Fill(255,255,255,1);
		//2 bits of text
		TextMid(width - clock_width*.1, height/140.0f, sync_text, SerifTypeface, height/70.0f);
		TextMid(width - clock_width*.1, height*4.0f/140.0f, "NTP-Derived Clock", SerifTypeface, height/70.0f);
		Translate(text_width + clock_width/2.0f, height/2.0f);
		//Write out hour labels around edge of clock
		for(i = 0; i < 12; i++)
		{
			char buf[5];
			sprintf(buf, "%d", i? i:12);
			TextMid(hours_y[i],hours_x[i], buf, SansTypeface, height/15.0f);
		}
		//Go around drawing dashes around edge of clock
		StrokeWidth(clock_width/100.0f);
		VGfloat start = height *7.5f/20.0f;
		VGfloat end_short = height *6.7f/20.0f;
		VGfloat end_long = height *6.3f/20.0f;
#ifndef NO_COLOUR_CHANGE
		Stroke(255,0,0,1);
#endif
		//As we go around we actually keep drawing in the same place every time, but we keep rotating the co-ordinate space around the centre point of the clock...
		for(i = 0; i < 60; i++)
		{
			if((i %5) == 0)
				Line(0, start, 0, end_long);
			else
				Line(0, start, 0, end_short);
			Rotate(-6);
#ifndef NO_COLOUR_CHANGE
			//Fade red slowly out over first second
			if(i == tm_now.tm_sec)
			{
				if(i < 1)
				{
					//Example to fade over 2 seconds...
					//VGfloat fade = 128.0f * tm_now.tm_sec +  127.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
					VGfloat fade = 255.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
					Stroke(255, fade, fade, 1);
				}
				else
					Stroke(255,255,255,1);
			}
#endif
		}
		//Again, rotate co-ordinate space so we're just drawing an upright line every time...
		StrokeWidth(clock_width/200.0f);
		//VGfloat sec_rotation = -(6.0f * tm_now.tm_sec +((VGfloat)tval.tv_usec*6.0f/1000000.0f));
		VGfloat sec_rotation = -(6.0f * tm_now.tm_sec);
		VGfloat sec_part = sec_rotation;
		if(tval.tv_usec > MOVE_HAND_AT)
			sec_rotation -= ((VGfloat)(tval.tv_usec - MOVE_HAND_AT)*6.0f/100000.0f);
		sec_part -= ((VGfloat)tval.tv_usec)*6.0f/1000000.0f;
		VGfloat min_rotation = -6.0f *tm_now.tm_min + sec_part/60.0f;
		VGfloat hour_rotation = -30.0f *tm_now.tm_hour + min_rotation/12.0f;
		Rotate(hour_rotation);
		Line(0,0,0,height/4.0f); /* half-length hour hand */
		Rotate(min_rotation - hour_rotation);
		Line(0,0,0,height/2.0f); /* minute hand */
		Rotate(sec_rotation - min_rotation);
		Stroke(255,0,0,1);
		Line(0,0,0,height/2.0f); /* second hand */
		Rotate(-sec_rotation);
		//Now draw some dots for seconds...
#ifdef SECOND_DOTS
		StrokeWidth(clock_width/100.0f);
		Stroke(0,0,255,1);
		VGfloat pos = height*7.9f/20.0f;
		for(i = 0; i < 60; i++)
		{
			if(i <= tm_now.tm_sec)
			{
				Line(0, pos, 0, pos -10);
			}
			Rotate(-6);
		}
#endif

		End();						   			// End the picture
	}

	bRunning = 0;
	finish();					            // Graphics cleanup
	exit(0);
}
