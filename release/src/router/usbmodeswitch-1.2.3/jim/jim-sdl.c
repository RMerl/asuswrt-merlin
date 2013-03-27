
/* Jim - SDL extension
 * Copyright 2005 Salvatore Sanfilippo <antirez@invece.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * A copy of the license is also included in the source distribution
 * of Jim, as a TXT file name called LICENSE.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

#include "jim.h"
#include "jimautoconf.h"

#define AIO_CMD_LEN 128

typedef struct JimSdlSurface
{
    SDL_Surface *screen;
} JimSdlSurface;

static void JimSdlSetError(Jim_Interp *interp)
{
    Jim_SetResultString(interp, SDL_GetError(), -1);
}

static void JimSdlDelProc(Jim_Interp *interp, void *privData)
{
    JimSdlSurface *jss = privData;

    JIM_NOTUSED(interp);

    SDL_FreeSurface(jss->screen);
    Jim_Free(jss);
}

/* Calls to commands created via [sdl.surface] are implemented by this
 * C command. */
static int JimSdlHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss = Jim_CmdPrivData(interp);
    int option;
    static const char * const options[] = {
        "free", "flip", "pixel", "rectangle", "box", "line", "aaline",
        "circle", "aacircle", "fcircle", NULL
    };
    enum
    { OPT_FREE, OPT_FLIP, OPT_PIXEL, OPT_RECTANGLE, OPT_BOX, OPT_LINE,
        OPT_AALINE, OPT_CIRCLE, OPT_AACIRCLE, OPT_FCIRCLE
    };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }
    if (Jim_GetEnum(interp, argv[1], options, &option, "SDL surface method", JIM_ERRMSG) != JIM_OK)
        return JIM_ERR;
    if (option == OPT_PIXEL) {
        /* PIXEL */
        long x, y, red, green, blue, alpha = 255;

        if (argc != 7 && argc != 8) {
            Jim_WrongNumArgs(interp, 2, argv, "x y red green blue ?alpha?");
            return JIM_ERR;
        }
        if (Jim_GetLong(interp, argv[2], &x) != JIM_OK ||
            Jim_GetLong(interp, argv[3], &y) != JIM_OK ||
            Jim_GetLong(interp, argv[4], &red) != JIM_OK ||
            Jim_GetLong(interp, argv[5], &green) != JIM_OK ||
            Jim_GetLong(interp, argv[6], &blue) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 8 && Jim_GetLong(interp, argv[7], &alpha) != JIM_OK)
            return JIM_ERR;
        pixelRGBA(jss->screen, x, y, red, green, blue, alpha);
        return JIM_OK;
    }
    else if (option == OPT_RECTANGLE || option == OPT_BOX ||
        option == OPT_LINE || option == OPT_AALINE) {
        /* RECTANGLE, BOX, LINE, AALINE */
        long x1, y1, x2, y2, red, green, blue, alpha = 255;

        if (argc != 9 && argc != 10) {
            Jim_WrongNumArgs(interp, 2, argv, "x y red green blue ?alpha?");
            return JIM_ERR;
        }
        if (Jim_GetLong(interp, argv[2], &x1) != JIM_OK ||
            Jim_GetLong(interp, argv[3], &y1) != JIM_OK ||
            Jim_GetLong(interp, argv[4], &x2) != JIM_OK ||
            Jim_GetLong(interp, argv[5], &y2) != JIM_OK ||
            Jim_GetLong(interp, argv[6], &red) != JIM_OK ||
            Jim_GetLong(interp, argv[7], &green) != JIM_OK ||
            Jim_GetLong(interp, argv[8], &blue) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 10 && Jim_GetLong(interp, argv[9], &alpha) != JIM_OK)
            return JIM_ERR;
        switch (option) {
            case OPT_RECTANGLE:
                rectangleRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
            case OPT_BOX:
                boxRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
            case OPT_LINE:
                lineRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
            case OPT_AALINE:
                aalineRGBA(jss->screen, x1, y1, x2, y2, red, green, blue, alpha);
                break;
        }
        return JIM_OK;
    }
    else if (option == OPT_CIRCLE || option == OPT_AACIRCLE || option == OPT_FCIRCLE) {
        /* CIRCLE, AACIRCLE, FCIRCLE */
        long x, y, radius, red, green, blue, alpha = 255;

        if (argc != 8 && argc != 9) {
            Jim_WrongNumArgs(interp, 2, argv, "x y radius red green blue ?alpha?");
            return JIM_ERR;
        }
        if (Jim_GetLong(interp, argv[2], &x) != JIM_OK ||
            Jim_GetLong(interp, argv[3], &y) != JIM_OK ||
            Jim_GetLong(interp, argv[4], &radius) != JIM_OK ||
            Jim_GetLong(interp, argv[5], &red) != JIM_OK ||
            Jim_GetLong(interp, argv[6], &green) != JIM_OK ||
            Jim_GetLong(interp, argv[7], &blue) != JIM_OK) {
            return JIM_ERR;
        }
        if (argc == 9 && Jim_GetLong(interp, argv[8], &alpha) != JIM_OK)
            return JIM_ERR;
        switch (option) {
            case OPT_CIRCLE:
                circleRGBA(jss->screen, x, y, radius, red, green, blue, alpha);
                break;
            case OPT_AACIRCLE:
                aacircleRGBA(jss->screen, x, y, radius, red, green, blue, alpha);
                break;
            case OPT_FCIRCLE:
                filledCircleRGBA(jss->screen, x, y, radius, red, green, blue, alpha);
                break;
        }
        return JIM_OK;
    }
    else if (option == OPT_FREE) {
        /* FREE */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        Jim_DeleteCommand(interp, Jim_String(argv[0]));
        return JIM_OK;
    }
    else if (option == OPT_FLIP) {
        /* FLIP */
        if (argc != 2) {
            Jim_WrongNumArgs(interp, 2, argv, "");
            return JIM_ERR;
        }
        SDL_Flip(jss->screen);
        return JIM_OK;
    }
    return JIM_OK;
}

static int JimSdlSurfaceCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimSdlSurface *jss;
    char buf[AIO_CMD_LEN];
    Jim_Obj *objPtr;
    long screenId, xres, yres;
    SDL_Surface *screen;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "xres yres");
        return JIM_ERR;
    }
    if (Jim_GetLong(interp, argv[1], &xres) != JIM_OK ||
        Jim_GetLong(interp, argv[2], &yres) != JIM_OK)
        return JIM_ERR;

    /* Try to create the surface */
    screen = SDL_SetVideoMode(xres, yres, 32, SDL_SWSURFACE | SDL_ANYFORMAT);
    if (screen == NULL) {
        JimSdlSetError(interp);
        return JIM_ERR;
    }
    /* Get the next file id */
    if (Jim_EvalGlobal(interp, "if {[catch {incr sdl.surfaceId}]} {set sdl.surfaceId 0}") != JIM_OK)
        return JIM_ERR;
    objPtr = Jim_GetVariableStr(interp, "sdl.surfaceId", JIM_ERRMSG);
    if (objPtr == NULL)
        return JIM_ERR;
    if (Jim_GetLong(interp, objPtr, &screenId) != JIM_OK)
        return JIM_ERR;

    /* Create the SDL screen command */
    jss = Jim_Alloc(sizeof(*jss));
    jss->screen = screen;
    sprintf(buf, "sdl.surface%ld", screenId);
    Jim_CreateCommand(interp, buf, JimSdlHandlerCommand, jss, JimSdlDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;
}

int Jim_sdlInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "sdl", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        JimSdlSetError(interp);
        return JIM_ERR;
    }
    atexit(SDL_Quit);
    Jim_CreateCommand(interp, "sdl.screen", JimSdlSurfaceCommand, NULL, NULL);
    return JIM_OK;
}
