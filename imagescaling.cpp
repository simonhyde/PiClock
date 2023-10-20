
#include "imagescaling.h"
#include <cstring>
#include "globals.h"
//Static member variables of ResizeQueue
std::queue<std::shared_ptr<ResizedImage> > ResizeQueue::m_queue;
std::mutex ResizeQueue::m_access_mutex;
std::condition_variable ResizeQueue::m_new_data_condition;

ResizedImage::ResizedImage(const Magick::Geometry & geom, std::shared_ptr<Magick::Image> & pSrc, const std::string & name, bool quick)
:pSource(pSrc),Geom(geom),Name(name), bQuick(quick)
{}
void ResizedImage::DoResize()
{
    static Magick::Color black = Magick::Color(0,0,0,0);
    Magick::Image scaled(*pSource);
    scaled.flip();
    if(bQuick)
        scaled.filterType(Magick::PointFilter);
    scaled.resize(Geom);
    scaled.extent(Geom, black, Magick::CenterGravity);
    pOutput = std::make_shared<Magick::Blob>();
    scaled.write(pOutput.get(), "RGBA", 8);
}

void ResizeQueue::Add(const std::shared_ptr<ResizedImage> & pMsg)
{
    std::lock_guard<std::mutex> hold_lock(m_access_mutex);
    m_queue.push(pMsg);
    m_new_data_condition.notify_all();
}
std::shared_ptr<ResizedImage> ResizeQueue::Get(bool &bRunning)
{
    std::unique_lock<std::mutex> hold_lock(m_access_mutex);
    while(bRunning && m_queue.empty())
    {
        m_new_data_condition.wait(hold_lock);
    }
    if(bRunning)
    {
        auto front = m_queue.front();
        m_queue.pop();
        return front;
    }
    return std::shared_ptr<ResizedImage>();
}
void ResizeQueue::Abort()
{
    m_new_data_condition.notify_all();
}

void ResizeQueue::RunBackgroundResizeThread(bool &bRunning)
{
    while(bRunning)
    {
        auto data = Get(bRunning);
        if(data)
        {
            data->DoResize();
            //If this is a quick resize, then schedule a slow resize too
            if(data->bQuick)
                Add(std::make_shared<ResizedImage>(data->Geom, data->pSource, data->Name, false));
            msgQueue.Add(data);
        }
    }
}


ScalingImage::ScalingImage(std::shared_ptr<Magick::Image> pSrc, std::shared_ptr<Magick::Blob> pBlob)
    :pSource(pSrc), pSourceBlob(pBlob)
{}
//Empty constructor required for std::map, shouldn't get called;
ScalingImage::ScalingImage()
{}
bool ScalingImage::IsValid() const
{
    return (bool)pSource;
}
int ScalingImage::GetImage(NVGcontext* vg, int w, int h, const std::string & name)
{
    savedVg = vg;
    if(!IsValid())
        return 0;
    Magick::Geometry geom(w, h);
    const auto & iter = Scaled.find(geom);
    if(iter != Scaled.end())
    {
        if(iter->second.blob)
        {
            if(iter->second.handle == 0)
                iter->second.handle = nvgCreateImageRGBA(vg, w, h, 0, (const unsigned char *)iter->second.blob->data());
            return iter->second.handle;
        }
    }
    else
    {
        std::shared_ptr<ResizedImage> pResize = std::make_shared<ResizedImage>(geom, pSource, name, true);
        Scaled[geom] = ScaledData();
        ResizeQueue::Add(pResize);
    }
    return 0;
}
void ScalingImage::UpdateFromResize(std::shared_ptr<ResizedImage> pResize)
{
    //Refuse to do this if our image has changed since then...
    if(pResize && pResize->pOutput && pResize->pSource == pSource)
    {
        const auto & geom = pResize->Geom;
        const auto & iter = Scaled.find(geom);
        if(iter == Scaled.end())
        {
            //Impossible happened, data is created before job is scheduled
            return;
        }
        if(savedVg != NULL && iter->second.handle != 0)
        {
            nvgDeleteImage(savedVg, iter->second.handle);
        }
        iter->second.handle = 0;
        iter->second.blob = pResize->pOutput;
    }
}
bool ScalingImage::IsSameSource(std::shared_ptr<Magick::Blob> pOtherBlob)
{
    return IsValid() && pSourceBlob && pOtherBlob 
        && (pOtherBlob->length() == pSourceBlob->length())
        && (memcmp(pOtherBlob->data(), pSourceBlob->data(), pSourceBlob->length()) == 0);
}
ScalingImage::~ScalingImage()
{
    if(savedVg == NULL)
        return;
    for(const auto &item : Scaled)
    {
        nvgDeleteImage(savedVg, item.second.handle);
    }
}

