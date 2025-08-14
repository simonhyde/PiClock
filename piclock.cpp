// OpenVG Clock
// Simon Hyde <simon.hyde@bbc.co.uk>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <fstream>
#include <map>
#include <queue>
#include <cctype>
#include <memory>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <boost/program_options.hpp>
#include <netdb.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <Magick++.h>
#include <nanovg.h>
#include <fcntl.h>
#include "ntpstat/ntpstat.h"
#include "piclock_messages.h"
#include "nvg_main.h"
#include "globals.h"
#include "control_tcp.h"
#include "nvg_helpers.h"
#include "fonts.h"
#include "nvg_helpers.h"
#include "tally.h"
#include "countdownclock.h"
#include "regionstate.h"
#include "imagescaling.h"
#include "overallstate.h"
#include "analogueclock.h"
#include "gpio.h"


#define FPS 25
#define FRAMES 0

namespace po = boost::program_options;

static OverallState globalState;


int GPI_MODE = 0;
int GPIO_TYPE = 0;
std::string GPIO_PULLS;
int init_window_width = 0;
int init_window_height = 0;
std::string clean_exit_file("/tmp/piclock_clean_exit");

void DrawFrame(NVGcontext *vg, int iwidth, int iheight);
void NvgInit(NVGcontext *vg);




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






void read_settings(const std::string & filename,
		   po::variables_map& vm)
{
	po::options_description desc("Options");
	desc.add_options()
		("init_window_width", po::value<int>(&init_window_width)->default_value(0), "Initial window width, specifying 0 gives fullscreen mode")
		("init_window_height", po::value<int>(&init_window_height)->default_value(0), "Initial window height, specifying 0 gives fullscreen mode")
		("gpio_mode", po::value<int>(&GPIO_TYPE)->default_value(0), "GPIO Type, 0=PiFace Digital, 1=Raspberry Pi (not yet implemented)")
		("gpio_pulls", po::value<std::string>(&GPIO_PULLS)->default_value("UUUUUUUU"), "GPI Pull Up/Down/Off status")
		("tally_mode", po::value<int>(&GPI_MODE)->default_value(0), "Tally Mode, 0=disabled, 1=GPI/O, 2=TCP/IP, 3=TCP/IP with GPIO status passed back to controller")
		("tally_remote_host", po::value<std::vector<std::string>>(&tally_hosts), "Remote tally host, may be specified multiple times for multiple connections")
		("tally_remote_port", po::value<std::string>(&TALLY_SERVICE)->default_value("6254"), "Port (or service) to connect to on (default 6254)")
		("tally_shared_secret", po::value<std::string>(&TALLY_SECRET)->default_value("SharedSecretGoesHere"), "Shared Secret (password) for connecting to tally service")
		("clean_exit_file", po::value<std::string>(&clean_exit_file)->default_value("/tmp/piclock_clean_exit"), "Flag file created to indicate a clean exit (from keyboard request)")
	;
	
	std::ifstream settings_file(filename.c_str());
	
	vm = po::variables_map();

	po::store(po::parse_config_file(settings_file, desc), vm);
	po::notify(vm);
}

void cleanup()
{
	bRunning = false;
	ResizeQueue::Abort();

	//If there's a clean exit trigger file, create it
	if(!clean_exit_file.empty())
		open(clean_exit_file.c_str(), O_CREAT, 00666);
	exit(0);
}

void KeyCallback(unsigned char key, int x, int y)
{
	if(key == 'q')
	{
		cleanup();
	}
}


/* All variables which used to be local to main(), which we've had
   to make global now that drawing is done via a callback
 */
static struct timeval tval;
static ntpstate_t ntp_state_data;
static int vrate;
static int hrate;

int main(int argc, char *argv[]) {
	po::variables_map vm;

	std::string configFile = "/etc/piclock.cfg";

	
	if(argc > 1)
		configFile = argv[1];
	read_settings(configFile, vm);

	gettimeofday(&tval, NULL);//Just for handle_clock_messages on first pass


	srand(time(NULL));
	vrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	hrate = 1800 + (((VGfloat)rand())*1800.0f)/RAND_MAX;
	init_ntp_state();
	get_ntp_state(&ntp_state_data);
	pthread_t ntp_thread;
	pthread_attr_t ntp_attr;
	pthread_attr_init(&ntp_attr);
	pthread_create(&ntp_thread, &ntp_attr, &ntp_check_thread, &ntp_state_data);
	std::thread resize_thread(ResizeQueue::RunBackgroundResizeThread, std::ref(bRunning));
	struct sched_param resize_param;
	resize_param.sched_priority = sched_get_priority_min(SCHED_IDLE);
	pthread_setschedparam(resize_thread.native_handle(), SCHED_IDLE, &resize_param);
	if(GPI_MODE & 1)
                gpio_init(GPIO_TYPE, GPIO_PULLS);
	if(GPI_MODE & 2)
		create_tcp_threads();

	nvg_main(DrawFrame, NvgInit, init_window_width, init_window_height);

	//Shouldn't ever get here, but no harm in cleaning up anyway
	cleanup();
}

void NvgInit(NVGcontext *vg)
{
    //Called at NVG startup before loop

    //Mostly just loads default fonts
    globalState.NvgInit(vg);
}
void DrawFrame(NVGcontext *vg, int iwidth, int iheight)
{
	static VGfloat offset = iheight*.05f;

	static time_t tm_last_comms_good = 0;

	static VGfloat h_offset_pos = 0;
	static VGfloat v_offset_pos = 0;

	static long prev_sec = 0;

	static bool bRecalcTextsNext = true;

	auto now = std::chrono::system_clock::now();

	RotateTextClipCache();

	if(globalState.Regions.empty())
		//Add 1 region, for when we have no TCP connection, should probably only happen on first pass
		globalState.Regions[0] = std::make_shared<RegionState>();
	//Handle any queued messages
	std::queue<std::shared_ptr<ClockMsg> > newMsgs;
	msgQueue.Get(newMsgs);
	bool bRecalcTexts = globalState.HandleClockMessages(vg, newMsgs, tval) || bRecalcTextsNext;
	bRecalcTextsNext = false;
	
	bool bDigitalClockPrefix = RegionState::DigitalClockPrefix(globalState.Regions);

	//nvgTransform(vg, 1,iheight,0,-1,0,0);
	//Start(iwidth, iheight);					// Start the picture
	VGfloat display_width = iwidth;
	VGfloat display_height = iheight;
	if(globalState.ScreenSaver())
	{
		display_width -= offset;
		display_height -= offset;
	}
	if(globalState.RotationReqd(display_width,display_height))
	{
		Rotate(vg,90);
		//After rotating our output has now disappeared down below our co-ordinate space, so translate our co-ordinate space down to find it...
		nvgTranslate(vg,0,-iwidth);
		std::swap(display_width,display_height);
	}
	
	//Screen saver offsets...
	if(globalState.ScreenSaver())
	{
		if(tval.tv_sec != prev_sec)
		{
			prev_sec =  tval.tv_sec;
			h_offset_pos = abs(prev_sec%(hrate*2) - hrate)*offset/hrate;
			v_offset_pos = abs(prev_sec%(vrate*2) - vrate)*offset/vrate;
		}
		nvgTranslate(vg, h_offset_pos, v_offset_pos);
	}
	for(const auto & region : globalState.Regions)
	{
		auto &RS = *region.second;
		VGfloat inner_height = display_height * RS.height();
		VGfloat inner_width = display_width * RS.width();

		bRecalcTexts = RS.RecalcDimensions(vg, globalState, now, inner_width, inner_height, display_width, display_height, region.first == 0, bDigitalClockPrefix) || bRecalcTexts;
	}
	if(bRecalcTexts)
	{
		globalState.TextSizes.clear();
		globalState.LabelSizes.clear();
		for(const auto & region : globalState.Regions)
		{
			auto &RS = *region.second;
			RS.SetDefaultZone("R" + std::to_string(region.first));
			RS.RecalcTexts(vg, globalState, tval);
		}
	}

	for(const auto & region : globalState.Regions)
	{
		std::string profName;
		std::shared_ptr<RegionState> pRS = region.second;

		VGfloat return_x = display_width *pRS->x();
		VGfloat return_y = display_height *(pRS->top_y());
		nvgSave(vg);//Save current translation to stack to be restored later
		nvgTranslate(vg, return_x, return_y);

		nvgFillColor(vg, colWhite);					// white text
		nvgStrokeColor(vg, colWhite);				// White strokes
		nvgStrokeWidth(vg, 0);//Don't draw strokes until we've moved onto the analogue clock

		//Write out the text
		DisplayBox db;
		std::string prefix;
		pRS->DrawDigitals(vg, globalState, bDigitalClockPrefix, now);
		pRS->DrawDate(vg, globalState, now);

		if(GPI_MODE == 1)
		{
			uint16_t gpis = read_gpi();
			if(pRS->TD.nRows != 2)
				bRecalcTextsNext = true;
			pRS->TD.nRows = 2;
			pRS->TD.nCols_default = 1;
			pRS->TD.nCols.clear();
			uint8_t colour_weight = (gpis & 1) ? 50:255;
			uint8_t fill_weight = (gpis & 1)? 50:255;
			pRS->TD.displays[0][0] = std::make_shared<SimpleTallyState>(
				    TallyColour(fill_weight, fill_weight, fill_weight),
				    TallyColour(colour_weight, colour_weight*0.55,0),
				    "Mic Live");
			colour_weight = (gpis & 2) ? 50:255;
			fill_weight = (gpis & 2)? 50:255;
			pRS->TD.displays[1][0] = std::make_shared<SimpleTallyState>(
				    TallyColour(fill_weight, fill_weight, fill_weight),
				    TallyColour(colour_weight,0,0),
				    "On Air");
		}
                else if(GPI_MODE == 3)
                {
			update_tcp_gpis(read_gpi());
                }

		if(pRS->HasStatusBox())
		{
			bool bAnyComms = pRS->DrawStatusArea(vg, ntp_state_data.status, tval.tv_sec % 2, (GPI_MODE & 2)? tally_hosts.size() : 0, bComms, mac_address, globalState.FontStatus());
			if(bAnyComms)
			{
				tm_last_comms_good = tval.tv_sec;
			}
			//Stop showing stuff after 5 seconds of comms failed...
			else if(tm_last_comms_good < tval.tv_sec - 5)
			{
				for(auto & reg: globalState.Regions)
				{
					reg.second->TD.clear();
				}
			}
		}
		//Draw Tally Displays
		if(GPI_MODE)
		{
			pRS->DrawTallies(vg, globalState, tval);
		}
		//Right, now start drawing the clock if it exists in our region
		pRS->DrawAnalogueClock(vg, now, globalState.FontHours(), globalState.Images);
		//Translate back to the origin...
		nvgRestore(vg);
	}
}
