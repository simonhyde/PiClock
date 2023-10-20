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
#include "ntpstat/ntpstat.h"
//piface digital
#include "pifacedigital.h"
#include "piclock_messages.h"
#include "nvg_main.h"
#include "blocking_tcp_client.h"
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


#define FPS 25
#define FRAMES 0
//Number of micro seconds into a second to start moving the second hand
#define MOVE_HAND_AT	900000

namespace po = boost::program_options;

int GPI_MODE = 0;
int init_window_width = 0;
int init_window_height = 0;
std::string clean_exit_file("/tmp/piclock_clean_exit");

void DrawFrame(NVGcontext *vg, int iwidth, int iheight);




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
		("tally_mode", po::value<int>(&GPI_MODE)->default_value(0), "Tally Mode, 0=disabled, 1=PiFace Digital, 2=TCP/IP")
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

	std::string configFile = "piclock.cfg";

	
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
	if(GPI_MODE == 1)
		pifacedigital_open(0);
	else if(GPI_MODE == 2)
		create_tcp_threads();

	nvg_main(DrawFrame, init_window_width, init_window_height);

	//Shouldn't ever get here, but no harm in cleaning up anyway
	cleanup();
}
void DrawFrame(NVGcontext *vg, int iwidth, int iheight)
{
	static VGfloat commsWidth = -1;
	static VGfloat commsTextHeight = 0;
	static RegionsMap regions;
	static VGfloat offset = iheight*.05f;

	static std::map<std::string, int> textSizes;
	static std::map<std::string, int> labelSizes;

	static time_t tm_last_comms_good = 0;

	static ImagesMap images;

	static VGfloat h_offset_pos = 0;
	static VGfloat v_offset_pos = 0;

	static long prev_sec = 0;

	static struct tm tm_local, tm_utc;

	static bool bRecalcTextsNext = true;

	static NVGcolor colWhite = nvgRGBf(1.0f,1.0f,1.0f);
	static NVGcolor colMidGray = nvgRGBf(0.5f,0.5f,0.5f);
	//Unused at the moment, uncomment to re-enable
	//static NVGcolor colBlack = nvgRGBf(0.0f,0.0f,0.0f);
	static NVGcolor colRed = nvgRGBf(1.0f,0.0f,0.0f);
	//Unused, at the moment, uncomment to re-enable
	//static NVGcolor colBlue = nvgRGBf(0.0f,0.0f,1.0f);
        static NVGcolor colNtpSynced = nvgRGB(0,100,0);
        static NVGcolor colNtpNotSync[2] = {nvgRGB(120,0,120),nvgRGB(120,0,0)};
	static NVGcolor colNtpText = nvgRGB(200,200,200);
	static NVGcolor colCommsOk = nvgRGB(0,100,0);
	static NVGcolor colCommsFail = nvgRGB(190,0,0);

	RotateTextClipCache();

	if(regions.empty())
		//Add 1 region, for when we have no TCP connection, should probably only happen on first pass
		regions[0] = std::make_shared<RegionState>();
	//Handle any queued messages
	std::queue<std::shared_ptr<ClockMsg> > newMsgs;
	msgQueue.Get(newMsgs);
	bool bRecalcTexts = handle_clock_messages(newMsgs, regions, images, tval) || bRecalcTextsNext;
	bRecalcTextsNext = false;
	
	bool bDigitalClockPrefix = RegionState::DigitalClockPrefix(regions);

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
	bool bFirst = true;
	for(const auto & region : regions)
	{
		auto &RS = *region.second;
		VGfloat inner_height = display_height * RS.height();
		VGfloat inner_width = display_width * RS.width();

		bRecalcTexts = RS.RecalcDimensions(vg, tm_utc, tm_local, inner_width, inner_height, display_width, display_height, bFirst, bDigitalClockPrefix) || bRecalcTexts;
		bFirst = false;
	}
	if(bRecalcTexts)
	{
		textSizes.clear();
		labelSizes.clear();
		for(const auto & region : regions)
		{
			auto &RS = *region.second;
			RS.SetDefaultZone("R" + std::to_string(region.first));
			DisplayBox db = RS.TallyBox();
			VGfloat row_height = (db.h)/((VGfloat)RS.TD.nRows);
			VGfloat col_width_default = db.w/((VGfloat)RS.TD.nCols_default);

			for(int row = 0; row < RS.TD.nRows; row++)
			{
				int col_count = RS.TD.nCols[row];
				VGfloat col_width = col_width_default;
				if(col_count <= 0)
					col_count = RS.TD.nCols_default;
				else
					col_width = db.w/((VGfloat)col_count);
				for(int col = 0; col < col_count; col++)
				{
					const std::string & zone = RS.GetZone(row, col);
					auto item = RS.TD.displays[row][col];
					if(!item)
						continue;
					auto label = item->Label(tval);
					auto text = item->Text(tval);
					auto iter = images.end();
					if(text && ((iter = images.find(*text)) == images.end() || !iter->second.IsValid()))
					{
						auto maxItemSize = MaxPointSize(vg, col_width * .9f, row_height * (label? .6f :.9f), item->Text(tval)->c_str(), FONT(item->IsMonoSpaced()));
						if(textSizes[zone] == 0 || textSizes[zone] > maxItemSize)
							textSizes[zone] = maxItemSize;
						if(label)
						{
							auto maxLabelSize = MaxPointSize(vg, -1, row_height *.2f, label->c_str(), FONT(false));
							if(labelSizes[zone] == 0 || labelSizes[zone] > maxLabelSize)
								labelSizes[zone] = maxLabelSize;
						}
					}
				}
			}
		}
		commsWidth = -1;
	}
	gettimeofday(&tval, NULL);
	localtime_r(&tval.tv_sec, &tm_local);
	gmtime_r(&tval.tv_sec, &tm_utc);
	for(const auto & region : regions)
	{
		std::string profName;
		int i;
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
		int pointSize;
		std::string prefix;
		if(pRS->DigitalLocal(db, pointSize, prefix))
		{
			std::string time_str = pRS->FormatTime(tm_local, tval.tv_usec);
			if(bDigitalClockPrefix)
				time_str = prefix + time_str;
			db.TextMidBottom(vg, time_str, FONT_MONO, pointSize);
		}
		if(pRS->DigitalUTC(db, pointSize, prefix))
		{
			std::string time_str = pRS->FormatTime(tm_utc, tval.tv_usec);
			if(bDigitalClockPrefix)
				time_str = prefix + time_str;
			db.TextMidBottom(vg, time_str, FONT_MONO, pointSize);
		}
		bool bLocal;
		if(pRS->Date(db, bLocal, pointSize))
		{
			db.TextMidBottom(vg, pRS->FormatDate(bLocal? tm_local: tm_utc),
						FONT_PROP, pointSize);
		}

		if(GPI_MODE == 1)
		{
			if(pRS->TD.nRows != 2)
				bRecalcTextsNext = true;
			pRS->TD.nRows = 2;
			pRS->TD.nCols_default = 1;
			pRS->TD.nCols.empty();
			uint8_t gpis = pifacedigital_read_reg(INPUT,0);
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

		//Draw NTP sync status
		db = pRS->StatusBox(pointSize);
		if(db.w > 0.0f && db.h > 0.0f)
		{
			nvgFillColor(vg,colWhite);
			const char * sync_text;
			if(ntp_state_data.status == 0)
			{
				nvgFillColor(vg,colNtpSynced);
				sync_text = "Synchronised";
			}
			else
			{
				/* flash between red and purple once a second */
				nvgFillColor(vg,colNtpNotSync[(tval.tv_sec %2)]);
				if(ntp_state_data.status == 1)
					sync_text = "Synchronising..";
				else
					sync_text = "Unknown Synch!";
			}
			const char * header_text = "NTP-Derived Clock";
			auto statusBoxLen = std::max(TextWidth(vg, sync_text, FONT_PROP, pointSize),
						     TextWidth(vg, header_text, FONT_PROP, pointSize))*1.1f;
			auto statusTextHeight = TextHeight(vg, FONT_PROP, pointSize);
			//Draw a box around NTP status
			Rect(vg, db.x + db.w - statusBoxLen, db.top_y() , statusBoxLen, db.h);
			nvgFillColor(vg, colNtpText);
			//2 bits of text
			TextMid(vg, db.x + db.w - statusBoxLen*.5f, db.y - statusTextHeight *.8f, sync_text, FONT_PROP, pointSize);
			TextMid(vg, db.x + db.w - statusBoxLen*.5f, db.y - statusTextHeight*2.f, header_text, FONT_PROP, pointSize);

			db.w -= statusBoxLen;

			if(GPI_MODE == 2)
			{
				//Draw comms status
				if(commsWidth < 0)
				{
					commsWidth =
						std::max(TextWidth(vg, "Comms OK", FONT_PROP, pointSize),

							 TextWidth(vg, "Comms Failed", FONT_PROP, pointSize));
					for(unsigned int window = 0; window < tally_hosts.size(); window++)
					{
						char buf[512];
						buf[511] = '\0';
						snprintf(buf, 511, "Tally Server %d", window + 1);
						commsWidth = std::max(commsWidth, TextWidth(vg, buf, FONT_PROP, pointSize));
					}
					commsWidth *= 1.2f;
					commsTextHeight = TextHeight(vg, FONT_PROP, pointSize);
				}
				bool bAnyComms = false;
				for(unsigned int window = 0; window < tally_hosts.size(); window++)
				{
					bAnyComms = bAnyComms || bComms[window];
					if(bComms[window])
						nvgFillColor(vg,colCommsOk);
					else
						nvgFillColor(vg,colCommsFail);
					VGfloat base_x = db.x + db.w - commsWidth * (VGfloat)(tally_hosts.size()-window);
					Rect(vg, base_x, db.top_y(), commsWidth, db.h);
					nvgFillColor(vg,colNtpText);
					char buf[512];
					buf[511] = '\0';
					base_x += commsWidth*.5f;
					snprintf(buf, 511, "Tally Server %d", window + 1);
					TextMid(vg, base_x, db.y - commsTextHeight*2.0f, buf, FONT_PROP, pointSize);
					snprintf(buf, 511, "Comms %s", bComms[window]? "OK" : "Failed");
					TextMid(vg, base_x, db.y - commsTextHeight*0.8f, buf, FONT_PROP, pointSize);
				}
				if(bAnyComms)
				{
					tm_last_comms_good = tval.tv_sec;
				}
				//Stop showing stuff after 5 seconds of comms failed...
				else if(tm_last_comms_good < tval.tv_sec - 5)
				{
					for(auto & reg: regions)
					{
						reg.second->TD.clear();
					}
				}
			}
		}
		if(GPI_MODE)
		{
			profName = pRS->TD.sProfName;
			if(pRS->TD.nRows > 0)
			{
				DisplayBox db = pRS->TallyBox();
				VGfloat row_height = (db.h)/((VGfloat)pRS->TD.nRows);
				VGfloat buffer = row_height*.02f;
				VGfloat col_width_default = db.w/((VGfloat)pRS->TD.nCols_default);
				//float y_offset = height/10.0f;
				for(int row = 0; row < pRS->TD.nRows; row++)
				{
					VGfloat base_y = db.y - ((VGfloat)row)*row_height;
					int col_count = pRS->TD.nCols[row];
					VGfloat col_width = col_width_default;
					if(col_count <= 0)
						col_count = pRS->TD.nCols_default;
					else
						col_width = db.w/((VGfloat)col_count);
					for(int col = 0; col < col_count; col++)
					{
						auto curTally = pRS->TD.displays[row][col];
						if(!curTally)
							continue;
						DisplayBox dbTally(((VGfloat)col)*col_width + db.w/100.0f, base_y, col_width - buffer, row_height - buffer);
						auto text = curTally->Text(tval);
						auto iter = images.end();
						int img_handle = 0;
						if(text && (iter = images.find(*text)) != images.end() && iter->second.IsValid())
						{
							int w = (int)dbTally.w;
							int h = (int)dbTally.h;
							img_handle = iter->second.GetImage(vg, w, h, *text);
							if(img_handle != 0)
								drawimage(vg, dbTally.x, dbTally.top_y(), w, h, img_handle);
							else
								text = std::shared_ptr<std::string>();
						}
						if(img_handle == 0)
						{
							curTally->BG(tval)->Fill(vg);
							dbTally.Roundrect(vg, row_height/10.0f);
							curTally->FG(tval)->Fill(vg);
							const auto & zone = pRS->GetZone(row,col);
							dbTally.TextMid(vg, text, FONT(curTally->IsMonoSpaced()), textSizes[zone], labelSizes[zone], curTally->Label(tval));
						}
					}
				}
			}
		}
		//Done with displays, back to common code...
		db = pRS->StatusBox(pointSize);
		if(db.w > 0.0f && db.h > 0.0f && GPI_MODE == 2)
		{
			nvgFillColor(vg, colMidGray);
			char buf[8192];
			buf[sizeof(buf)-1] = '\0';
			if(profName.empty())
				snprintf(buf, sizeof(buf) -1, "MAC Address %s", mac_address.c_str());
			else
				snprintf(buf, sizeof(buf) -1, "MAC Address %s, %s", mac_address.c_str(), profName.c_str());
			Text(vg, db.x, db.y, buf, FONT_PROP, pointSize);
		}
		nvgFillColor(vg,colWhite);
		int numbers;
		std::shared_ptr<const std::map<int, VGfloat>> hours_x;
		std::shared_ptr<const std::map<int, VGfloat>> hours_y;
		if(pRS->AnalogueClock(db, bLocal, hours_x, hours_y, numbers))
		{
			//Right, now start drawing the clock

			//Move to the centre of the clock
			VGfloat move_x = db.mid_x();
			VGfloat move_y = db.mid_y();
			
			nvgTranslate(vg, move_x, move_y);

			if(numbers)
			{
				//Write out hour labels around edge of clock
				for(i = 0; i < 12; i++)
				{
					char buf[5];
					sprintf(buf, "%d", i? i:12);
					TextMid(vg, hours_x->at(i),hours_y->at(i), buf, FONT_HOURS, db.h/15.0f);
				}
			}
			//Spin clock around 180 degrees because this code originally worked with Y co-ordinates increasing as you went up
			Rotate(vg, 180.0f);
			//Go around drawing dashes around edge of clock
			nvgStrokeWidth(vg, db.w/100.0f);
			VGfloat start, end_short, end_long;
			VGfloat min_dim = std::min(db.h,db.w);
			if(numbers == 1)
			{
				start = min_dim *7.5f/20.0f;
				end_short = min_dim *6.7f/20.0f;
				end_long = min_dim *6.3f/20.0f;
			}
			else
			{
				start = min_dim *9.5f/20.0f;
				end_short = min_dim *8.8f/20.0f;
				end_long = min_dim *8.4f/20.0f;
			}
#ifndef NO_COLOUR_CHANGE
			nvgStrokeColor(vg,colRed);
#endif
#define tm_now	(bLocal? tm_local : tm_utc)
			//As we go around we actually keep drawing in the same place every time, but we keep rotating the co-ordinate space around the centre point of the clock...
			for(i = 0; i < 60; i++)
			{
				if((i %5) == 0)
					Line(vg, 0, start, 0, end_long);
				else
					Line(vg, 0, start, 0, end_short);
				Rotate(vg, 6);
#ifndef NO_COLOUR_CHANGE
				//Fade red slowly out over first second
				if(i == tm_now.tm_sec)
				{
					if(i < 1)
					{
						//Example to fade over 2 seconds...
						//VGfloat fade = 128.0f * tm_now.tm_sec +  127.0f * ((VGfloat)tval.tv_usec)/1000000.0f;
						VGfloat fade = ((VGfloat)tval.tv_usec)/200000.0f;
						if(fade > 1.0f)
							fade = 1.0f;
						nvgStrokeColor(vg, nvgRGBf(1.0f, fade, fade));
					}
					else
						nvgStrokeColor(vg, colWhite);
				}
#endif
			}
			nvgStrokeColor(vg, colWhite);
			//Again, rotate co-ordinate space so we're just drawing an upright line every time...
			nvgStrokeWidth(vg, db.w/200.0f);
			//VGfloat sec_rotation = -(6.0f * tm_now.tm_sec +((VGfloat)tval.tv_usec*6.0f/1000000.0f));
			VGfloat sec_rotation = (6.0f * tm_now.tm_sec);
			VGfloat sec_part = sec_rotation;
			if(tval.tv_usec > MOVE_HAND_AT)
				sec_rotation += ((VGfloat)(tval.tv_usec - MOVE_HAND_AT)*6.0f/100000.0f);
			//Take into account microseconds when calculating position of minute hand (and to minor extent hour hand), so it doesn't jump every second
			sec_part += ((VGfloat)tval.tv_usec)*6.0f/1000000.0f;
			VGfloat min_rotation = 6.0f *tm_now.tm_min + sec_part/60.0f;
			VGfloat hour_rotation = 30.0f *tm_now.tm_hour + min_rotation/12.0f;
			Rotate(vg, hour_rotation);
			Line(vg, 0,0,0,min_dim/4.0f); /* half-length hour hand */
			Rotate(vg, min_rotation - hour_rotation);
			Line(vg, 0,0,0,min_dim/2.0f); /* minute hand */
			Rotate(vg, sec_rotation - min_rotation);
			nvgStrokeColor(vg, colRed);
			Line(vg, 0,-db.h/10.0f,0,min_dim/2.0f); /* second hand, with overhanging tail */
			//Draw circle in centre
			nvgFillColor(vg, colRed);
			nvgBeginPath(vg);
			nvgCircle(vg, 0,0,db.w/150.0f);
			nvgFill(vg);
			Rotate(vg, -sec_rotation -180.0f);
			//Now draw some dots for seconds...
#ifdef SECOND_DOTS
			nvgStrokeWidth(vg, db.w/100.0f);
			nvgStrokeColor(vg, colBlue);
			VGfloat pos = db.h*7.9f/20.0f;
			for(i = 0; i < 60; i++)
			{
				if(i <= tm_now.tm_sec)
				{
					Line(vg, 0, pos, 0, pos -10);
				}
				Rotate(vg, -6);
			}
#endif

		}
		//Translate back to the origin...
		nvgRestore(vg);
	}
}
