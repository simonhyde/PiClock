#ifndef __PICLOCK_GLOBALS_H
#define __PICLOCK_GLOBALS_H

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include "piclock_messages.h"
#include "overallstate.h"

extern bool bRunning;

extern std::string TALLY_SERVICE;
extern std::string TALLY_SECRET;

extern std::string mac_address;

extern std::vector<std::string> tally_hosts;
extern std::map<unsigned int,bool> bComms;

extern MessageQueue msgQueue;
extern OverallState globalState;


#endif