#ifndef __PICLOCK_FONTS_H_INCLUDE
#define __PICLOCK_FONTS_H_INCLUDE
#ifdef __cplusplus
#include <string>
typedef std::string Fontinfo;
#endif
#define FONT_SERIF	"SerifTypeface__PiClock__"
#define FONT_SANS	"SansTypeface__PiClock__"
#define FONT_MONO	"MonoTypeface__PiClock__"
#define DEFAULT_FONT_TALLY	FONT_SERIF
#define DEFAULT_FONT_DATE	FONT_SERIF
#define DEFAULT_FONT_HOURS	FONT_SANS
#define DEFAULT_FONT_DIGITAL	FONT_MONO
#define DEFAULT_FONT_TALLYLABEL DEFAULT_FONT_TALLY
#define DEFAULT_FONT(x)		((x)? DEFAULT_FONT_DIGITAL : DEFAULT_FONT_TALLY)

#endif
