/***********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  See CREDITS file to find the copyright owners of this file.

  SDL Input/Audio/Video code (many lines of code come from snes9x & drnoksnes)
  (c) Copyright 2011         Makoto Sugano (makoto.sugano@gmail.com)

  Snes9x homepage: http://www.snes9x.com/

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
 ***********************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "controls.h"
#include "movie.h"
#include "logger.h"
#include "conffile.h"
#include "blit.h"
#include "display.h"

#include "sdl_snes9x.h"

typedef struct
{
	SDL_Window  	*sdlWindow;    // SDL2
	SDL_Texture     *sdlTexture;   // SDL2
    SDL_Renderer    *sdlRenderer;  // SDL2
	SDL_Surface     *sdl_screen;
	SDL_Rect		*p_screen_rect;
	SDL_Rect		sdl_screen_rect;
	uint8			*snes_buffer;
	uint8			*blit_screen;
	uint32			blit_screen_pitch;
	int			    video_mode;
    bool8           fullscreen;
	int 			screen_width;
	int 			screen_height;
} GUIData;

static GUIData GUI;

typedef	void (* Blitter) (uint8 *, int, uint8 *, int, int, int);

#ifdef __linux
// Select seems to be broken in 2.x.x kernels - if a signal interrupts a
// select system call with a zero timeout, the select call is restarted but
// with an infinite timeout! The call will block until data arrives on the
// selected fd(s).
//
// The workaround is to stop the X library calling select in the first
// place! Replace XPending - which polls for data from the X server using
// select - with an ioctl call to poll for data and then only call the blocking
// XNextEvent if data is waiting.
#define SELECT_BROKEN_FOR_SIGNALS
#endif

enum
{
	VIDEOMODE_BLOCKY = 1,
	VIDEOMODE_TV,
	VIDEOMODE_SMOOTH,
	VIDEOMODE_SUPEREAGLE,
	VIDEOMODE_2XSAI,
	VIDEOMODE_SUPER2XSAI,
	VIDEOMODE_EPX,
	VIDEOMODE_HQ2X
};

static void SetupImage (void);
static void TakedownImage (void);

void S9xExtraDisplayUsage (void)
{
	S9xMessage(S9X_INFO, S9X_USAGE, "-fullscreen                     fullscreen mode (without scaling)");
	S9xMessage(S9X_INFO, S9X_USAGE, "");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v1                             Video mode: Blocky (default)");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v2                             Video mode: TV");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v3                             Video mode: Smooth");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v4                             Video mode: SuperEagle");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v5                             Video mode: 2xSaI");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v6                             Video mode: Super2xSaI");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v7                             Video mode: EPX");
	S9xMessage(S9X_INFO, S9X_USAGE, "-v8                             Video mode: hq2x");
	S9xMessage(S9X_INFO, S9X_USAGE, "-res WIDTHxHEIGHT               Screen resolution");
	S9xMessage(S9X_INFO, S9X_USAGE, "");
}

void S9xParseDisplayArg (char **argv, int &i, int argc)
{
	if (!strncasecmp(argv[i], "-fullscreen", 11))
        {
                GUI.fullscreen = TRUE;
                printf ("Entering fullscreen mode (without scaling).\n");
        }
	else if (!strncasecmp(argv[i], "-v", 2))
	{
		switch (argv[i][2])
		{
			case '1':	GUI.video_mode = VIDEOMODE_BLOCKY;		break;
			case '2':	GUI.video_mode = VIDEOMODE_TV;			break;
			case '3':	GUI.video_mode = VIDEOMODE_SMOOTH;		break;
			case '4':	GUI.video_mode = VIDEOMODE_SUPEREAGLE;	break;
			case '5':	GUI.video_mode = VIDEOMODE_2XSAI;		break;
			case '6':	GUI.video_mode = VIDEOMODE_SUPER2XSAI;	break;
			case '7':	GUI.video_mode = VIDEOMODE_EPX;			break;
			case '8':	GUI.video_mode = VIDEOMODE_HQ2X;		break;
		}
	}
	else if (!strncasecmp(argv[i], "-res", 4))
	{
		char res_str[32];

		if ((i + 1) < argc)
		{
			if (strlen(argv[i+1]) < 32)
			{
				// Copy resolution setting to a buffer
				memset(res_str, 0, 32);
				strcpy(res_str, argv[i+1]);

				// Now it is safe to strtok() the parameter
				char *p_width = strtok(res_str, "x");
				char *p_height = strtok(NULL, "x");
				if ((p_width != NULL) && (p_height != NULL))
				{
					GUI.screen_width = atoi(p_width);
					GUI.screen_height = atoi(p_height);
				}
			}
		}
	}
	else
		S9xUsage();
}

const char * S9xParseDisplayConfig (ConfigFile &conf, int pass)
{
	if (pass != 1)
		return ("Unix/SDL");

	if (conf.Exists("Unix/SDL::VideoMode"))
	{
		GUI.video_mode = conf.GetUInt("Unix/SDL::VideoMode", VIDEOMODE_BLOCKY);
		if (GUI.video_mode < 1 || GUI.video_mode > 8)
			GUI.video_mode = VIDEOMODE_BLOCKY;
	}
	else
		GUI.video_mode = VIDEOMODE_BLOCKY;

	return ("Unix/SDL");
}

static void FatalError (const char *str)
{
	fprintf(stderr, "%s\n", str);
	S9xExit();
}

void S9xInitDisplay (int argc, char **argv)
{
	printf("Initializing SDL2...\n");
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		printf("Unable to initialize SDL: %s\n", SDL_GetError());
	}
	else
	{
		printf("SDL2 initialized.\n");
	}
  
	atexit(SDL_Quit);
	
	/*
	 * domaemon
	 *
	 * we just go along with RGB565 for now, nothing else..
	 */
	
	S9xSetRenderPixelFormat(RGB565);
	
	S9xBlitFilterInit();
	S9xBlit2xSaIFilterInit();
	S9xBlitHQ2xFilterInit();

	// Check screen resolution, given as parameter earlier
	if ((GUI.screen_width == 0) || (GUI.screen_height == 0))
	{
		GUI.screen_width = SNES_WIDTH * 2;
		GUI.screen_height = SNES_HEIGHT_EXTENDED * 2; 
	}

	if (GUI.fullscreen)
	{
		GUI.sdl_screen_rect.w = GUI.screen_height * (SNES_WIDTH * 2) / (SNES_HEIGHT_EXTENDED * 2); 
		GUI.sdl_screen_rect.h = GUI.screen_height; 
		GUI.sdl_screen_rect.x = (GUI.screen_width - GUI.sdl_screen_rect.w) / 2; 
		GUI.sdl_screen_rect.y = 0; 
		GUI.p_screen_rect = &GUI.sdl_screen_rect; 
	}
	else
	{
		GUI.p_screen_rect = NULL;	// This will stretch the texture to fill the window
	}


	GUI.sdlWindow = SDL_CreateWindow("Snes9x",
						SDL_WINDOWPOS_CENTERED,
						SDL_WINDOWPOS_CENTERED,
						GUI.screen_width, GUI.screen_height,
						0);

	if (GUI.sdlWindow == NULL)
	{
		printf("Unable to create SDL window: %s\n", SDL_GetError());
		exit(1);
	}


	GUI.sdlRenderer = SDL_CreateRenderer(GUI.sdlWindow, -1, SDL_RENDERER_ACCELERATED);

	if (GUI.sdlRenderer == NULL)
	{
		printf("Unable to create SDL renderer: %s\n", SDL_GetError());
		exit(1);
	}

	GUI.sdl_screen = SDL_CreateRGBSurface(0, SNES_WIDTH * 2, SNES_HEIGHT_EXTENDED * 2, 32,
											0x00FF0000,
											0x0000FF00,
											0x000000FF,
											0xFF000000);

	GUI.sdlTexture = SDL_CreateTexture(GUI.sdlRenderer,
										SDL_PIXELFORMAT_RGB565,
										SDL_TEXTUREACCESS_TARGET,
										SNES_WIDTH * 2, SNES_HEIGHT_EXTENDED);

	/*
	 * domaemon
	 *
	 * buffer allocation, quite important
	 */
	SetupImage();
}

void S9xDeinitDisplay (void)
{
	TakedownImage();

	SDL_Quit();

	S9xBlitFilterDeinit();
	S9xBlit2xSaIFilterDeinit();
	S9xBlitHQ2xFilterDeinit();
}

static void TakedownImage (void)
{
	if (GUI.snes_buffer)
	{
		free(GUI.snes_buffer);
		GUI.snes_buffer = NULL;
	}

	S9xGraphicsDeinit();
}

static void SetupImage (void)
{
	TakedownImage();

    SDL_SetRenderDrawColor(GUI.sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(GUI.sdlRenderer);
    SDL_RenderPresent(GUI.sdlRenderer);

	// domaemon: The whole unix code basically assumes output=(original * 2);
	// This way the code can handle the SNES filters, which does the 2X.
	GFX.Pitch = SNES_WIDTH * 2 * 2;
	GUI.snes_buffer = (uint8 *) calloc(GFX.Pitch * ((SNES_HEIGHT_EXTENDED + 4) * 2), 1);
	if (!GUI.snes_buffer)
		FatalError("Failed to allocate GUI.snes_buffer.");

	// domaemon: Add 2 lines before drawing.
	GFX.Screen = (uint16 *) (GUI.snes_buffer + (GFX.Pitch * 2 * 2));
	GUI.blit_screen       = (uint8 *) GUI.sdl_screen->pixels;
	GUI.blit_screen_pitch = SNES_WIDTH * 2 * 2; // window size =(*2); 2 byte pir pixel =(*2)

	S9xGraphicsInit();
}

void S9xPutImage (int width, int height)
{
	static int	prevWidth = 0, prevHeight = 0;
	Blitter		blitFn = NULL;

	if (GUI.video_mode == VIDEOMODE_BLOCKY || GUI.video_mode == VIDEOMODE_TV || GUI.video_mode == VIDEOMODE_SMOOTH)
		if ((width <= SNES_WIDTH) && ((prevWidth != width) || (prevHeight != height)))
			S9xBlitClearDelta();

	if (width <= SNES_WIDTH)
	{
		if (height > SNES_HEIGHT_EXTENDED)
		{
			blitFn = S9xBlitPixSimple2x1;
		}
		else
		{
			switch (GUI.video_mode)
			{
				case VIDEOMODE_BLOCKY:		blitFn = S9xBlitPixSimple2x2;		break;
				case VIDEOMODE_TV:			blitFn = S9xBlitPixTV2x2;			break;
				case VIDEOMODE_SMOOTH:		blitFn = S9xBlitPixSmooth2x2;		break;
				case VIDEOMODE_SUPEREAGLE:	blitFn = S9xBlitPixSuperEagle16;	break;
				case VIDEOMODE_2XSAI:		blitFn = S9xBlitPix2xSaI16;			break;
				case VIDEOMODE_SUPER2XSAI:	blitFn = S9xBlitPixSuper2xSaI16;	break;
				case VIDEOMODE_EPX:			blitFn = S9xBlitPixEPX16;			break;
				case VIDEOMODE_HQ2X:		blitFn = S9xBlitPixHQ2x16;			break;
			}
		}
	}
	else
	if (height <= SNES_HEIGHT_EXTENDED)
	{
		switch (GUI.video_mode)
		{
			default:					blitFn = S9xBlitPixSimple1x2;	break;
			case VIDEOMODE_TV:			blitFn = S9xBlitPixTV1x2;		break;
		}
	}
	else
	{
		blitFn = S9xBlitPixSimple1x1;
	}


	// domaemon: this is place where the rendering buffer size should be changed?
	blitFn((uint8 *) GFX.Screen, GFX.Pitch, GUI.blit_screen, GUI.blit_screen_pitch, width, height);

	// domaemon: does the height change on the fly?
	if (height < prevHeight)
	{
		int	p = GUI.blit_screen_pitch >> 2;
        
		for (int y = SNES_HEIGHT * 2; y < SNES_HEIGHT_EXTENDED * 2; y++)
		{
			uint32	*d = (uint32 *) (GUI.blit_screen + y * GUI.blit_screen_pitch);
			for (int x = 0; x < p; x++)
				*d++ = 0;
		}
	}

    SDL_UpdateTexture(GUI.sdlTexture, NULL, GUI.sdl_screen->pixels, GUI.sdl_screen->pitch);
    SDL_RenderClear(GUI.sdlRenderer);
    SDL_RenderCopy(GUI.sdlRenderer, GUI.sdlTexture, NULL, GUI.p_screen_rect);
    SDL_RenderPresent(GUI.sdlRenderer);

	prevWidth  = width;
	prevHeight = height;
}

void S9xMessage (int type, int number, const char *message)
{
	const int	max = 36 * 3;
	static char	buffer[max + 1];

	fprintf(stdout, "%s\n", message);
	strncpy(buffer, message, max + 1);
	buffer[max] = 0;
	S9xSetInfoString(buffer);
}

const char * S9xStringInput (const char *message)
{
	static char	buffer[256];

	printf("%s: ", message);
	fflush(stdout);

	if (fgets(buffer, sizeof(buffer) - 2, stdin))
		return (buffer);

	return (NULL);
}

void S9xSetTitle (const char *string)
{
	// NOTE: SDL1 function: SDL_WM_SetCaption(string, string);
}

void S9xSetPalette (void)
{
	return;
}
