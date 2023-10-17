//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#define GLFW_INCLUDE_ES2
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include "nanovg.h"
#define NANOVG_GLES2_IMPLEMENTATION
#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"
//#include "perf.h"
#include "nvg_main.h"


void errorcb(int error, const char* desc)
{
	printf("GLFW error %d: %s\n", error, desc);
}


static void key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	NVG_NOTUSED(scancode);
	NVG_NOTUSED(mods);
	if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

int nvg_main(void (*drawFrame)(NVGcontext*,int,int), int init_window_width, int init_window_height)
{
	GLFWwindow* window;
	NVGcontext* vg = NULL;
	//PerfGraph fps;

	if (!glfwInit()) {
		printf("Failed to init GLFW.");
		return -1;
	}

	//initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");

	glfwSetErrorCallback(errorcb);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

        if(init_window_width == 0 || init_window_height == 0)
        {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);
            window = glfwCreateWindow(vidMode->width, vidMode->height, "PiClock", monitor, NULL);
        }
        else
        {
            window = glfwCreateWindow(init_window_width, init_window_height, "PiClock", NULL, NULL);
        }
	if (!window) {
		glfwTerminate();
		return -1;
	}

	glfwSetKeyCallback(window, key);

	glfwMakeContextCurrent(window);

	vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
	if (vg == NULL) {
		printf("Could not init nanovg.\n");
		return -1;
	}

	glfwSwapInterval(0);

	glfwSetTime(0);

	nvgCreateFont(vg, "SerifTypeface","/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf");
	nvgCreateFont(vg, "SansTypeface","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
	nvgCreateFont(vg, "MonoTypeface","/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");

	while (!glfwWindowShouldClose(window))
	{
		double mx, my;
		int winWidth, winHeight;
		int fbWidth, fbHeight;
		float pxRatio;

		glfwGetCursorPos(window, &mx, &my);
		glfwGetWindowSize(window, &winWidth, &winHeight);
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
		// Calculate pixel ration for hi-dpi devices.
		pxRatio = (float)fbWidth / (float)winWidth;

		// Update and render
		glViewport(0, 0, fbWidth, fbHeight);
		glClearColor(0,0,0,0);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		nvgBeginFrame(vg, winWidth, winHeight, pxRatio);

		drawFrame(vg, winWidth,winHeight);

		nvgEndFrame(vg);

		glEnable(GL_DEPTH_TEST);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	nvgDeleteGLES2(vg);

	glfwTerminate();
	return 0;
}
