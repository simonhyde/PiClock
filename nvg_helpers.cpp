#include "nvg_helpers.h"
#include <map>
#include <cmath>
#include <memory>

static std::map<Fontinfo,float> fontHeights;
static std::shared_ptr<std::map<std::pair<std::string, float>, std::string> > clippedTextCache;
static std::shared_ptr<std::map<std::pair<std::string, float>, std::string> > prevClippedTextCache;

void Rotate(NVGcontext *vg, float degrees)
{
	nvgRotate(vg, degrees* M_PI / 180.0f);
}

void Roundrect(NVGcontext *vg, float x, float y, float w, float h, float r)
{
	nvgBeginPath(vg);
	nvgRoundedRect(vg,x,y,w,h,r);
	nvgFill(vg);
}

void Rect(NVGcontext *vg, float x, float y, float w, float h)
{
	nvgBeginPath(vg);
	nvgRect(vg,x,y,w,h);
	nvgFill(vg);
}

void Line(NVGcontext *vg, float x1, float y1, float x2, float y2)
{
	nvgBeginPath(vg);
	nvgMoveTo(vg,x1,y1);
	nvgLineTo(vg,x2,y2);
	nvgStroke(vg);
}

float TextHeight(NVGcontext *vg, const Fontinfo &f, int pointsize)
{
	static unsigned char *allChars = NULL;
	auto iter = fontHeights.find(f);
	float retBase;
	if(iter == fontHeights.end())
	{
		if(allChars == NULL)
		{
			allChars = new unsigned char[256];
			for(unsigned char i = 0; i < 255; i++)
			{
				allChars[255-i] = i;
			}
			allChars[0] = 255;
		}
		float bounds[4] = {0,0,0,0};
		nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
		nvgFontFace(vg, f.c_str());
		//nvgTextBounds seems to round to the nearest whole number, so go for a really big font size for our cache, powers of 2 are easier to divide down accurately
		nvgFontSize(vg, 1024);
		nvgTextBounds(vg, 0,0, (char *)allChars, NULL, bounds);
		retBase = fontHeights[f] = bounds[3];
	}
	else
	{
		retBase = iter->second;
	}
	return (retBase * (float)pointsize)/1024.f;
}

float TextWidth(NVGcontext *vg, const char * str, const Fontinfo &f, int pointsize)
{
	float bounds[4] = {0,0,0,0};
	nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextBounds(vg, 0,0, str, NULL, bounds);
	return abs(bounds[2]);
}

void TextMid(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize)
{
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
	nvgText(vg, x, y, s, NULL);
}

void Text(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize)
{
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
	nvgText(vg, x, y, s, NULL);
}

void drawimage(NVGcontext *vg, float x, float y, float w, float h, int img_handle)
{
	NVGpaint imgPaint = nvgImagePattern(vg, x, y, w, h,0,img_handle,1.0f);
	nvgBeginPath(vg);
	nvgRect(vg, x, y, w, h);
	nvgFillPaint(vg, imgPaint);
	nvgFill(vg);
}




void RotateTextClipCache()
{
	if(clippedTextCache)
	{
		prevClippedTextCache = clippedTextCache;
	}
	else
	{
		prevClippedTextCache = std::make_shared<std::map<std::pair<std::string, float>, std::string> >();
	}

	clippedTextCache = std::make_shared<std::map<std::pair<std::string, float>, std::string> >();
}


void TextClip(NVGcontext *vg, float x, float y, const std::string &s, const Fontinfo &f, int pointsize, float clipwidth, const std::string &clip_str)
{
	std::string output_str = s;
	bool bThisCache = true;
	const auto cacheKey = std::pair<std::string, float>(f + "_FONT_" + s, clipwidth/(float) pointsize);
	auto iter = clippedTextCache->find(cacheKey);
	if(iter == clippedTextCache->end())
	{
		bThisCache = false;
		iter = prevClippedTextCache->find(cacheKey);
	}
	if(iter == prevClippedTextCache->end())
	{
		//nvgTextBounds seems to round to the nearest whole number, so go for a really big font size for our calculations, powers of 2 are easier to divide/multiply accurately
		float calcWidth = 1024.f*clipwidth / (float)pointsize; //Genericise for all point sizes
		//Do the work...
		float baseWidth = TextWidth(vg, s.c_str(), f, 1024);
		if(baseWidth > calcWidth)
		{
			std::string final_clip_str = clip_str;
			if(final_clip_str.length() > 0)
			{
				float clipLen = TextWidth(vg, final_clip_str.c_str(), f, 1024);
				if(clipLen >= calcWidth)
					final_clip_str = std::string();
				else
					calcWidth -= clipLen;
			}
			float string_percentage = calcWidth / baseWidth;
			std::string::size_type newLen = (unsigned)(((float)s.length())*string_percentage);
			float curWidth;
			//First find longest string which is too long from this point
			do
			{
				output_str = s.substr(0,newLen);
				curWidth = TextWidth(vg, output_str.c_str(), f, 1024);
			}
			while(curWidth < calcWidth && (++newLen <= s.length()));
			//Then work down to first string that is short enough from this point
			do
			{
				output_str = s.substr(0,newLen);
				curWidth = TextWidth(vg, output_str.c_str(), f, 1024);
			}
			while(curWidth > calcWidth && (--newLen > 0));
			output_str += final_clip_str;
		}
	}
	else
	{
		output_str = iter->second;
	}
	//Item has been used, from prev cache or generated, so re-cache for next pass
	if(!bThisCache)
	{
		(*clippedTextCache)[cacheKey] = output_str;
	}
	Text(vg,x,y,output_str.c_str(),f,pointsize);
}

void TextMidBottom(NVGcontext *vg, float x, float y, const char* s, const Fontinfo &f, int pointsize)
{
	nvgFontFace(vg, f.c_str());
	nvgFontSize(vg, pointsize);
	nvgTextAlign(vg, NVG_ALIGN_CENTER|NVG_ALIGN_BOTTOM);
	nvgText(vg, x, y, s, NULL);
}

int MaxPointSize(NVGcontext *vg, float width, float height, const std::string & text, const Fontinfo & f)
{
	int ret = 1;
	bool bOverflowed = false;
	nvgFontFace(vg, f.c_str());
	while(!bOverflowed)
	{
		ret++;
		float bounds[4] = {0,0,0,0};//xmin,ymin,xmax,ymax
		nvgTextAlign(vg, NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
		nvgFontSize(vg, ret);
		nvgTextBounds(vg, 0, 0, text.c_str(), NULL, bounds);
		if(abs(bounds[3/*ymax*/]) > height)
			bOverflowed = true;
		else if(width > 0.0f && abs(bounds[2/*xmax*/]) > width)
			bOverflowed = true;
	}
	return ret - 1;
}

