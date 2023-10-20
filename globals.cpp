#include "globals.h"
#include <mutex>


std::string TALLY_SERVICE("6254");
std::string TALLY_SECRET("SharedSecretGoesHere");
bool bRunning = true;
std::string mac_address;
std::vector<std::string> tally_hosts;
std::map<unsigned int,bool> bComms;
MessageQueue msgQueue;

