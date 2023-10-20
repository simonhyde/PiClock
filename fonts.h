#ifndef __PICLOCK_FONTS_H_INCLUDE
#define __PICLOCK_FONTS_H_INCLUDE
#include <string>
typedef std::string Fontinfo;
#define FONT_PROP	"SerifTypeface"
#define FONT_HOURS	"SansTypeface"
#define FONT_MONO	"MonoTypeface"
#define FONT(x)		((x)? FONT_MONO : FONT_PROP)

#endif