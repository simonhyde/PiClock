#ifndef __PICLOCK_IMAGE_SCALING_H_INCLUDED
#define __PICLOCK_IMAGE_SCALING_H_INCLUDED

#include <map>
#include <memory>
#include <string>
#include <condition_variable>
#include <mutex>
#include "piclock_messages.h"

class ScalingImage;
typedef std::map<std::string, ScalingImage> ImagesMap;

class ResizedImage: public ClockMsg
{
public:
    std::shared_ptr<Magick::Image> pSource;
    Magick::Geometry Geom;
    std::shared_ptr<Magick::Blob> pOutput;
    std::string Name;
    bool bQuick;
	ResizedImage(const Magick::Geometry & geom, std::shared_ptr<Magick::Image> & pSrc, const std::string & name, bool quick);
	void DoResize();
};

class ResizeQueue
{
public:
	static void Add(const std::shared_ptr<ResizedImage> & pMsg);
	static void Abort();
    static void RunBackgroundResizeThread(bool &bRunning);
private:
	static std::shared_ptr<ResizedImage> Get(bool &bRunning);
	static std::queue<std::shared_ptr<ResizedImage> > m_queue;
	static std::mutex m_access_mutex;
	static std::condition_variable m_new_data_condition;
};


class ScalingImage
{
public:
	ScalingImage(std::shared_ptr<Magick::Image> pSrc, std::shared_ptr<Magick::Blob> pBlob);
	//Empty constructor required for std::map, shouldn't get called;
	ScalingImage();
	bool IsValid() const;
	int GetImage(NVGcontext* vg, int w, int h, const std::string & name);
	void UpdateFromResize(std::shared_ptr<ResizedImage> pResize);
	bool IsSameSource(std::shared_ptr<Magick::Blob> pOtherBlob);
	~ScalingImage();
private:
	class ScaledData
	{
		public:
			ScaledData() : handle(0)
			{}
			std::shared_ptr<Magick::Blob> blob;
			int handle;
	};
	NVGcontext *savedVg = NULL;
	std::shared_ptr<Magick::Image> pSource;
	std::shared_ptr<Magick::Blob> pSourceBlob;
	std::map<Magick::Geometry, ScaledData> Scaled;
};

#endif