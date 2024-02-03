/*
    Created on: Oct 18, 2019

	Copyright 2019 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#if defined(SUPPORT_X11) && !defined(USE_SDL) && !defined(LIBRETRO)
#include "cfg/option.h"
#include "types.h"
#include "xgl.h"

XGLGraphicsContext theGLContext;

static int x11_error_handler(Display *, XErrorEvent *)
{
	return 0;
}

bool XGLGraphicsContext::init()
{
	instance = this;
	int context_attribs[] =
	{
			GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
			GLX_CONTEXT_MINOR_VERSION_ARB, 3,
#ifndef NDEBUG
			GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
			GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
			None
	};
	int (*old_handler)(Display *, XErrorEvent *) = XSetErrorHandler(&x11_error_handler);

	context = glXCreateContextAttribsARB((Display *)display, *framebufferConfigs, 0, True, context_attribs);
	if (!context)
	{
		INFO_LOG(RENDERER, "OpenGL 4.3 not supported");
		// Try GL 3.0
		context_attribs[1] = 3;
		context_attribs[3] = 0;
		context = glXCreateContextAttribsARB((Display *)display, *framebufferConfigs, 0, True, context_attribs);
		if (!context)
		{
			ERROR_LOG(RENDERER, "OpenGL 3.0 not supported");
			return false;
		}
	}
	XSetErrorHandler(old_handler);
	XSync((Display *)display, False);

	glXMakeCurrent((Display *)display, (GLXDrawable)window, context);

	if (!gladLoadGL((GLADloadfunc) glXGetProcAddressARB))
		return false;

	if (!GLAD_GL_VERSION_3_1)
		return false;

	Window win;
	int temp;
	unsigned int tempu;
	XGetGeometry((Display *)display, (GLXDrawable)window, &win, &temp, &temp, (u32 *)&settings.display.width, (u32 *)&settings.display.height, &tempu, &tempu);

	swapOnVSync = config::VSync;
	if (glXSwapIntervalMESA != nullptr)
		glXSwapIntervalMESA((unsigned)swapOnVSync);
	else
	{
		if (glXSwapIntervalEXT != nullptr)
			glXSwapIntervalEXT((Display *)display, (GLXDrawable)window, (int)swapOnVSync);
	}

	postInit();

	return true;
}

bool XGLGraphicsContext::ChooseVisual(Display* x11Display, XVisualInfo** visual, int* depth)
{
	const long x11Screen = XDefaultScreen(x11Display);

	gladLoaderLoadGLX(x11Display, x11Screen);

	// Get a matching FB config
	static int visual_attribs[] =
	{
		GLX_X_RENDERABLE    , True,
		GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE     , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_ALPHA_SIZE      , 8,
		GLX_DOUBLEBUFFER    , True,
		None
	};

	int glx_major, glx_minor;

	// FBConfigs were added in GLX version 1.3.
	if (!glXQueryVersion(x11Display, &glx_major, &glx_minor) ||
			((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
	{
		ERROR_LOG(RENDERER, "Invalid GLX version");
		return false;
	}

	int fbcount;
	framebufferConfigs = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
	if (framebufferConfigs == nullptr)
	{
		ERROR_LOG(RENDERER, "Failed to retrieve a framebuffer config");
		return false;
	}
	INFO_LOG(RENDERER, "Found %d matching FB configs.", fbcount);

	// Get a visual
	XVisualInfo *vi = glXGetVisualFromFBConfig(x11Display, *framebufferConfigs);
	INFO_LOG(RENDERER, "Chosen visual ID = 0x%lx", vi->visualid);

	*depth = vi->depth;
	*visual = vi;

	return true;
}

void XGLGraphicsContext::swap()
{
	do_swap_automation();
	if (swapOnVSync == (settings.input.fastForwardMode || !config::VSync))
	{
		swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
		if (glXSwapIntervalMESA != nullptr)
			glXSwapIntervalMESA((unsigned)swapOnVSync);
		else if (glXSwapIntervalEXT != nullptr)
			glXSwapIntervalEXT((Display *)display, (GLXDrawable)window, (int)swapOnVSync);
	}
	glXSwapBuffers((Display *)display, (GLXDrawable)window);

	Window win;
	int temp;
	unsigned int tempu;
	XGetGeometry((Display *)display, (GLXDrawable)window, &win, &temp, &temp, (u32 *)&settings.display.width, (u32 *)&settings.display.height, &tempu, &tempu);
}

void XGLGraphicsContext::term()
{
	preTerm();
	if (context)
	{
		glXMakeCurrent((Display *)display, None, NULL);
		glXDestroyContext((Display *)display, context);
		context = (GLXContext)0;
	}
}

#endif
