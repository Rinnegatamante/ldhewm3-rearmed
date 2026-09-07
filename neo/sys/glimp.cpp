/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include <SDL.h>

#include "sys/platform.h"
#include "sys/vita_alloc.h"
#include "framework/Licensee.h"

#include "renderer/tr_local.h"

#if defined(_WIN32) && defined(ID_ALLOW_TOOLS)
#include "sys/win32/win_local.h"
#include <SDL_syswm.h>
#endif

#ifdef VITA
#include <vitasdk.h>
extern "C" void vglSwapBuffers(GLboolean has_commondialog);
extern "C" GLboolean vglInitWithCustomThreshold(int pool_size, int width, int height, int ram_threshold, int cdram_threshold, int phycont_threshold, int cdlg_threshold, SceGxmMultisampleMode msaa);
extern "C" void *vglGetProcAddress(const char *name);
#endif

idCVar in_nograb("in_nograb", "0", CVAR_SYSTEM | CVAR_NOCHEAT, "prevents input grabbing");
idCVar r_waylandcompat("r_waylandcompat", "0", CVAR_SYSTEM | CVAR_NOCHEAT | CVAR_ARCHIVE, "wayland compatible framebuffer");

static bool grabbed = false;

#if SDL_VERSION_ATLEAST(2, 0, 0)
static SDL_Window *window = NULL;
static SDL_GLContext context = NULL;
#else
static SDL_Surface *window = NULL;
#define SDL_WINDOW_OPENGL SDL_OPENGL
#define SDL_WINDOW_FULLSCREEN SDL_FULLSCREEN
#endif

static void SetSDLIcon()
{
#ifndef VITA
	Uint32 rmask, gmask, bmask, amask;

	// ok, the following is pretty stupid.. SDL_CreateRGBSurfaceFrom() pretends to use a void* for the data,
	// but it's really treated as endian-specific Uint32* ...
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif

	#include "doom_icon.h" // contains the struct d3_icon

	SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)d3_icon.pixel_data, d3_icon.width, d3_icon.height,
			d3_icon.bytes_per_pixel*8, d3_icon.bytes_per_pixel*d3_icon.width,
			rmask, gmask, bmask, amask);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowIcon(window, icon);
#else
	SDL_WM_SetIcon(icon, NULL);
#endif

	SDL_FreeSurface(icon);
#endif
}

/*
===================
GLimp_Init
===================
*/
/*int crasher(unsigned int argc, void *argv) {
	uint32_t *nullppointer = NULL;
	for (;;) {
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons & SCE_CTRL_SELECT) *nullppointer = 0;
		sceKernelDelayThread(100);
	}
	return 0;
}*/

bool GLimp_Init(glimpParms_t parms) {
	//SceUID crasher_thread = sceKernelCreateThread("crasher", crasher, 0x40, 0x1000, 0, 0, NULL);
	//sceKernelStartThread(crasher_thread, 0, NULL);
	
	common->Printf("Initializing OpenGL subsystem\n");
#ifndef VITA
	assert(SDL_WasInit(SDL_INIT_VIDEO));

	Uint32 flags = SDL_WINDOW_OPENGL;

	if (parms.fullScreen)
		flags |= SDL_WINDOW_FULLSCREEN;

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 ); // Defaults to 24 which is not needed and fails on old Tegras
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,  1 );

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	//SDL_GL_SetAttribute(SDL_GL_STEREO, parms.stereo ? 1 : 0);
	common->Printf("multiSamples = %d", parms.multiSamples);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, parms.multiSamples ? 1 : 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, parms.multiSamples);

	window = SDL_CreateWindow(ENGINE_VERSION,
								SDL_WINDOWPOS_UNDEFINED,
								SDL_WINDOWPOS_UNDEFINED,
								parms.width, parms.height, flags);

	if (!window) {
		common->Printf("FAILED TO CREATE WINDOWS");
	}
	else
	{
		common->Printf("WINDOW CREATED OK");
	}

	context = SDL_GL_CreateContext(window);

	if (!context) {
		common->Printf("FAILED TO CREATE CONTEXT");
	}
	else
	{
		common->Printf("CONTEXT CREATED OK");
	}

	SDL_GetWindowSize(window, &glConfig.vidWidthReal, &glConfig.vidHeightReal);

	glConfig.isFullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == SDL_WINDOW_FULLSCREEN;
#else
	window = SDL_CreateWindow("dummy", 45, 0, 960, 544, SDL_WINDOW_FULLSCREEN);

	glConfig.vidWidthReal = 960;
	glConfig.vidHeightReal = 544;
	glConfig.isFullscreen = true;
	vglInitWithCustomThreshold(0, glConfig.vidWidthReal, glConfig.vidHeightReal, 10 * 1024 * 1024, 0, 0, 0, SCE_GXM_MULTISAMPLE_4X);
	Sys_EnableVitaGLAllocators();
#endif
	
	//common->Printf("Using %d color bits, %d depth, %d stencil display\n",
	//				channelcolorbits, tdepthbits, tstencilbits);

	glConfig.colorBits = 32;
	glConfig.depthBits = 32;
	glConfig.stencilBits = 8;

	glConfig.displayFrequency = 60;

#ifndef VITA
	if (!window) {
		common->Warning("No usable GL mode found: %s", SDL_GetError());
		return false;
	}
#endif
	GLimp_WindowActive(true);

	return true;
}

/*
===================
GLimp_SetScreenParms
===================
*/
bool GLimp_SetScreenParms(glimpParms_t parms) {
	common->DPrintf("TODO: GLimp_ActivateContext\n");
	return true;
}

/*
===================
GLimp_Shutdown
===================
*/
void GLimp_Shutdown() {
	common->Printf("Shutting down OpenGL subsystem\n");
#ifndef VITA
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (context) {
		SDL_GL_DeleteContext(context);
		context = NULL;
	}

	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}
#endif
#endif
}

/*
===================
GLimp_SwapBuffers
===================
*/
void GLimp_SwapBuffers() {
#ifndef VITA
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SwapWindow(window);
#else
	SDL_GL_SwapBuffers();
#endif
#else
	vglSwapBuffers(GL_FALSE);
#endif
}

/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma(unsigned short red[256], unsigned short green[256], unsigned short blue[256]) {
#ifndef VITA
	if (!window) {
		common->Warning("GLimp_SetGamma called without window");
		return;
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (SDL_SetWindowGammaRamp(window, red, green, blue))
#else
	if (SDL_SetGammaRamp(red, green, blue))
#endif
		common->Warning("Couldn't set gamma ramp: %s", SDL_GetError());
#endif
}

/*
=================
GLimp_ActivateContext
=================
*/
void GLimp_ActivateContext() {
#ifndef VITA
	//common->DPrintf("TODO: GLimp_ActivateContext\n");
	SDL_GL_MakeCurrent(window, context);
#endif
}

/*
=================
GLimp_DeactivateContext
=================
*/
void GLimp_DeactivateContext() {
#ifndef VITA
	//common->DPrintf("TODO: GLimp_DeactivateContext\n");
	SDL_GL_MakeCurrent(window, NULL);
#endif
}

/*
===================
GLimp_ExtensionPointer
===================
*/
#ifdef __ANDROID__
#include <dlfcn.h>
#endif
GLExtension_t GLimp_ExtensionPointer(const char *name) {
#ifndef VITA
	assert(SDL_WasInit(SDL_INIT_VIDEO));

#ifdef __ANDROID__
	static void *glesLib = NULL;

	if( !glesLib )
	{
	    int flags = RTLD_LOCAL | RTLD_NOW;
		glesLib = dlopen("libGLESv2_CM.so", flags);
		//glesLib = dlopen("libGLESv3.so", flags);
		if( !glesLib )
		{
			glesLib = dlopen("libGLESv2.so", flags);
		}
	}

	GLExtension_t ret =  (GLExtension_t)dlsym(glesLib, name);
	//common->Printf("GLimp_ExtensionPointer %s  %p\n",name,ret);
	return ret;
#endif

	return (GLExtension_t)SDL_GL_GetProcAddress(name);
#else
	return (GLExtension_t)vglGetProcAddress(name);
#endif
}

void GLimp_WindowActive(bool active)
{
	LOGI( "GLimp_WindowActive %d", active );

	tr.windowActive = active;

	if(!active)
	{
		tr.BackendThreadShutdown();
	}
}

void GLimp_GrabInput(int flags) {
	bool grab = flags & GRAB_ENABLE;

	if (grab && (flags & GRAB_REENABLE))
		grab = false;

	if (flags & GRAB_SETSTATE)
		grabbed = grab;

	if (in_nograb.GetBool())
		grab = false;
#ifndef VITA
	if (!window) {
		common->Warning("GLimp_GrabInput called without window");
		return;
	}
#endif
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_ShowCursor(flags & GRAB_HIDECURSOR ? SDL_DISABLE : SDL_ENABLE);
	SDL_SetRelativeMouseMode((grab && (flags & GRAB_HIDECURSOR)) ? SDL_TRUE : SDL_FALSE);
	SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
#else
	SDL_ShowCursor(flags & GRAB_HIDECURSOR ? SDL_DISABLE : SDL_ENABLE);
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
}
