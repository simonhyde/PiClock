#include "displaybox.h"

DisplayBox::DisplayBox()
    :x(0),y(0),w(0),h(0),m_corner(0)
{}
DisplayBox::DisplayBox(VGfloat _x, VGfloat _y, VGfloat _w, VGfloat _h)
    :x(_x),y(_y),w(_w),h(_h),m_corner(0)
{
}
VGfloat DisplayBox::top_y() const
{
    return y - h;
}
VGfloat DisplayBox::mid_x() const
{
    return x + w/2.0f;
}
VGfloat DisplayBox::mid_y() const
{
    return y - h/2.0f;
}
void DisplayBox::TextMid(NVGcontext * vg, const std::shared_ptr<std::string> &str, const Fontinfo & font, const int pointSize, int labelSize, std::shared_ptr<std::string> label)
{
    if(str)
        TextMid(vg,*str,font,pointSize,labelSize,label);
    else
        TextMid(vg,std::string(),font,pointSize,labelSize,label);

}
void DisplayBox::TextMid(NVGcontext *vg, const std::string & str, const Fontinfo & font, const int pointSize, int labelSize, std::shared_ptr<std::string> label)
{
    VGfloat labelHeight = 0.0f;
    if(label)
    {
        if(labelSize <= 0)
            labelSize = pointSize/3;
        labelHeight = TextHeight(vg, FONT_PROP, labelSize);
        auto label_y = y - h + labelHeight;
        ::TextClip(vg, x + m_corner, label_y,
            *label, FONT_PROP, labelSize, w - m_corner, "...");
    }
    auto text_height = TextHeight(vg, font, pointSize);
    VGfloat text_y = mid_y() + text_height *.1f + labelHeight*0.5f;
            
    ::TextMid(vg, mid_x(), text_y, str.c_str(), font, pointSize);
}
void DisplayBox::TextMidBottom(NVGcontext *vg, const std::string & str, const Fontinfo & font, const int pointSize, std::shared_ptr<std::string> label)
{
    if(label)
    {
        auto label_y = y - h + TextHeight(vg, FONT_PROP, pointSize/3);
        ::Text(vg, x + w*.05f, label_y,
            label->c_str(), FONT_PROP, pointSize/3);
    }
            
    ::TextMidBottom(vg, mid_x(), y - h *.05f, str.c_str(), font, pointSize);
}
void DisplayBox::Roundrect(NVGcontext *vg, VGfloat corner)
{
    m_corner = corner;
    ::Roundrect(vg, x, top_y(), w, h, corner);
}
void DisplayBox::Rect(NVGcontext * vg)
{
    m_corner = 0;
    ::Rect(vg, x, top_y(), w, h);
}
void DisplayBox::Zero()
{
    m_corner = x = y = w = h = 0;
}
