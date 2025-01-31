/* scrot_selection.c

Copyright 2020-2021  Daniel T. Borelli <daltomi@disroot.org>
Copyright 2021       Martin C <martincation@protonmail.com>
Copyright 2021       Peter Wu <peterwu@hotmail.com>
Copyright 2021       Wilson Smith <01wsmith+gh@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/*
    This file is part of the scrot project.
    Part of the code comes from the main.c file and maintains its authorship.
*/

#include "scrot.h"
#include "selection_classic.h"
#include "selection_edge.h"

struct Selection** selectionGet(void)
{
    static struct Selection* sel = NULL;
    return &sel;
}

static void selectionAllocate(void)
{
    struct Selection** sel = selectionGet();
    *sel = calloc(1, sizeof(**sel));
}

static void selectionDeallocate(void)
{
    struct Selection** sel = selectionGet();
    free(*sel);
    *sel = NULL;
}

static void createCursors(void)
{
    struct Selection* const sel = *selectionGet();

    assert(sel != NULL);

    if (opt.select == SELECTION_MODE_CAPTURE)
        sel->curCross = XCreateFontCursor(disp, XC_cross);
    else if (opt.select == SELECTION_MODE_HIDE)
        sel->curCross = XCreateFontCursor(disp, XC_spraycan);
    else // SELECTION_MODE_HOLE
        sel->curCross = XCreateFontCursor(disp, XC_target);
    sel->curAngleNE = XCreateFontCursor(disp, XC_ur_angle);
    sel->curAngleNW = XCreateFontCursor(disp, XC_ul_angle);
    sel->curAngleSE = XCreateFontCursor(disp, XC_lr_angle);
    sel->curAngleSW = XCreateFontCursor(disp, XC_ll_angle);
}

static void freeCursors(void)
{
    struct Selection* const sel = *selectionGet();

    assert(sel != NULL);

    XFreeCursor(disp, sel->curCross);
    XFreeCursor(disp, sel->curAngleNE);
    XFreeCursor(disp, sel->curAngleNW);
    XFreeCursor(disp, sel->curAngleSE);
    XFreeCursor(disp, sel->curAngleSW);
}

void selectionCalculateRect(int x0, int y0, int x1, int y1)
{
    struct SelectionRect* const rect = scrotSelectionGetRect();

    rect->x = x0;
    rect->y = y0;
    rect->w = x1 - x0;
    rect->h = y1 - y0;

    if (rect->w == 0)
        rect->w++;

    if (rect->h == 0)
        rect->h++;

    if (rect->w < 0) {
        rect->x += rect->w;
        rect->w = 0 - rect->w;
    }

    if (rect->h < 0) {
        rect->y += rect->h;
        rect->h = 0 - rect->h;
    }
}

void scrotSelectionCreate(void)
{
    selectionAllocate();

    struct Selection* const sel = *selectionGet();

    assert(sel != NULL);

    createCursors();

    if (!strncmp(opt.lineMode, LINE_MODE_CLASSIC, LINE_MODE_CLASSIC_LEN)) {
        sel->create = selectionClassicCreate;
        sel->draw = selectionClassicDraw;
        sel->motionDraw = selectionClassicMotionDraw;
        sel->destroy = selectionClassicDestroy;
    } else if (!strncmp(opt.lineMode, LINE_MODE_EDGE, LINE_MODE_EDGE_LEN)) {
        sel->create = selectionEdgeCreate;
        sel->draw = selectionEdgeDraw;
        sel->motionDraw = selectionEdgeMotionDraw;
        sel->destroy = selectionEdgeDestroy;
    } else {
        // It never happened, fix the options.c file
        assert(0);
    }

    sel->create();

    unsigned int const EVENT_MASK = ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;

    if ((XGrabPointer(disp, root, False, EVENT_MASK, GrabModeAsync,
             GrabModeAsync, root, sel->curCross, CurrentTime)
            != GrabSuccess)) {
        scrotSelectionDestroy();
        errx(EXIT_FAILURE, "couldn't grab pointer");
    }
}

void scrotSelectionDestroy(void)
{
    struct Selection* const sel = *selectionGet();
    XUngrabPointer(disp, CurrentTime);
    freeCursors();
    XSync(disp, True);
    sel->destroy();
    selectionDeallocate();
}

void scrotSelectionDraw(void)
{
    struct Selection const* const sel = *selectionGet();
    sel->draw();
}

void scrotSelectionMotionDraw(int x0, int y0, int x1, int y1)
{
    struct Selection const* const sel = *selectionGet();
    unsigned int const EVENT_MASK = ButtonMotionMask | ButtonReleaseMask;
    Cursor cursor = None;

    if (x1 > x0 && y1 > y0)
        cursor = sel->curAngleSE;
    else if (x1 > x0)
        cursor = sel->curAngleNE;
    else if (y1 > y0)
        cursor = sel->curAngleSW;
    else
        cursor = sel->curAngleNW;
    XChangeActivePointerGrab(disp, EVENT_MASK, cursor, CurrentTime);
    sel->motionDraw(x0, y0, x1, y1);
}

struct SelectionRect* scrotSelectionGetRect(void)
{
    return &(*selectionGet())->rect;
}

Status scrotSelectionCreateNamedColor(char const* nameColor, XColor* color)
{
    assert(nameColor != NULL);
    assert(color != NULL);

    return XAllocNamedColor(disp, XDefaultColormap(disp, DefaultScreen(disp)),
        nameColor, color, &(XColor) {});
}

void scrotSelectionGetLineColor(XColor* color)
{
    scrotSelectionSetDefaultColorLine();

    Status const ret = scrotSelectionCreateNamedColor(opt.lineColor, color);

    if (!ret) {
        scrotSelectionDestroy();
        errx(EXIT_FAILURE, "Error allocate color:%s", strerror(BadColor));
    }
}

void scrotSelectionSetDefaultColorLine(void)
{
    if (!opt.lineColor)
        opt.lineColor = "gray";
}

bool scrotSelectionGetUserSel(struct SelectionRect* selectionRect)
{
    static int xfd = 0;
    static int fdSize = 0;
    XEvent ev;
    fd_set fdSet;
    int count = 0, done = 0;
    int rx = 0, ry = 0, rw = 0, rh = 0, isButtonPressed = 0;
    Window target = None;
    Status ret;

    scrotSelectionCreate();

    xfd = ConnectionNumber(disp);
    fdSize = xfd + 1;

    ret = XGrabKeyboard(disp, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    if (ret == AlreadyGrabbed) {
        int attempts = 20;
        struct timespec delay = { 0, 50 * 1000L * 1000L };
        do {
            nanosleep(&delay, NULL);
            ret = XGrabKeyboard(disp, root, False, GrabModeAsync, GrabModeAsync, CurrentTime);
        } while (--attempts > 0 && ret == AlreadyGrabbed);
    }
    if (ret != GrabSuccess) {
        scrotSelectionDestroy();
        errx(EXIT_FAILURE, "failed to grab keyboard");
    }

    while (1) {
        /* Handle events here */
        while (!done && XPending(disp)) {
            XNextEvent(disp, &ev);
            switch (ev.type) {
            case MotionNotify:
                if (isButtonPressed)
                    scrotSelectionMotionDraw(rx, ry, ev.xbutton.x, ev.xbutton.y);
                break;
            case ButtonPress:
                isButtonPressed = 1;
                rx = ev.xbutton.x;
                ry = ev.xbutton.y;
                target = scrotGetWindow(disp, ev.xbutton.subwindow, ev.xbutton.x, ev.xbutton.y);
                if (target == None)
                    target = root;
                break;
            case ButtonRelease:
                done = 1;
                break;
            case KeyPress:
            {
                KeySym* keysym = NULL;
                int keycode; /*dummy*/

                keysym = XGetKeyboardMapping(disp, ev.xkey.keycode, 1, &keycode);

                if (!keysym)
                    break;

                if (!isButtonPressed) {
                key_abort_shot:
                    if (!opt.ignoreKeyboard || *keysym == XK_Escape) {
                        warnx("Key was pressed, aborting shot");
                        done = 2;
                    }
                    XFree(keysym);
                    break;
                }

                switch (*keysym) {
                case XK_Right:
                    if (++rx > scr->width)
                        rx = scr->width;
                    break;
                case XK_Left:
                    if (--rx < 0)
                        rx = 0;
                    break;
                case XK_Down:
                    if (++ry > scr->height)
                        ry = scr->height;
                    break;
                case XK_Up:
                    if (--ry < 0)
                        ry = 0;
                    break;
                default:
                    goto key_abort_shot;
                }
                XFree(keysym);
                scrotSelectionMotionDraw(rx, ry, ev.xbutton.x, ev.xbutton.y);
                break;
            }
            case KeyRelease:
                /* ignore */
                break;
            default:
                break;
            }
        }
        if (done)
            break;

        /* now block some */
        FD_ZERO(&fdSet);
        FD_SET(xfd, &fdSet);
        errno = 0;
        count = select(fdSize, &fdSet, NULL, NULL, NULL);
        if ((count < 0)
            && ((errno == ENOMEM) || (errno == EINVAL) || (errno == EBADF))) {
            scrotSelectionDestroy();
            errx(EXIT_FAILURE, "Connection to X display lost");
        }
    }
    scrotSelectionDraw();

    XUngrabKeyboard(disp, CurrentTime);

    bool const isAreaSelect = (scrotSelectionGetRect()->w > 5);

    scrotSelectionDestroy();

    if (done == 2)
        return false;

    if (isAreaSelect) {
        /* If a rect has been drawn, it's an area selection */
        rw = ev.xbutton.x - rx;
        rh = ev.xbutton.y - ry;

        if ((ev.xbutton.x + 1) == WidthOfScreen(scr))
            ++rw;

        if ((ev.xbutton.y + 1) == HeightOfScreen(scr))
            ++rh;

        if (rw < 0) {
            rx += rw;
            rw = 0 - rw;
        }

        if (rh < 0) {
            ry += rh;
            rh = 0 - rh;
        }

        // Not record pointer if there is a selection area because it is busy on that,
        // unless the delay option is used.
        if (opt.delay == 0)
            opt.pointer = 0;
    } else {
        /* else it's a window click */
        if (!scrotGetGeometry(target, &rx, &ry, &rw, &rh))
            return false;
    }
    scrotNiceClip(&rx, &ry, &rw, &rh);

    if (!opt.silent)
        XBell(disp, 0);

    selectionRect->x = rx;
    selectionRect->y = ry;
    selectionRect->w = rw;
    selectionRect->h = rh;
    return true;
}

Imlib_Image scrotSelectionSelectMode(void)
{
    struct SelectionRect rect0, rect1;

    int const oldSelect = opt.select;

    opt.select = SELECTION_MODE_CAPTURE;

    if (!scrotSelectionGetUserSel(&rect0))
        return NULL;

    opt.select = oldSelect;

    if (opt.select == SELECTION_MODE_HOLE
        || opt.select == SELECTION_MODE_HIDE)
        if (!scrotSelectionGetUserSel(&rect1))
            return NULL;

    scrotDoDelay();

    Imlib_Image capture = imlib_create_image_from_drawable(0, rect0.x, rect0.y,
        rect0.w, rect0.h, 1);

    if (opt.pointer)
       scrotGrabMousePointer(capture, rect0.x, rect0.y);

    if (opt.select == SELECTION_MODE_HOLE
        || opt.select == SELECTION_MODE_HIDE) {

        XColor color;
        scrotSelectionGetLineColor(&color);

        int const alpha = optionsParseRequireRange(opt.lineOpacity, 0, 255);
        int const x = rect1.x - rect0.x;
        int const y = rect1.y - rect0.y;

        imlib_context_set_image(capture);

        if (opt.select == SELECTION_MODE_HOLE) {
            Imlib_Image hole = imlib_clone_image();
            imlib_context_set_color(color.red, color.green, color.blue, alpha);
            imlib_image_fill_rectangle(0, 0, rect0.w, rect0.h);
            imlib_blend_image_onto_image(hole, 0, x, y, rect1.w, rect1.h, x, y, rect1.w, rect1.h);
            imlib_context_set_image(hole);
            imlib_free_image_and_decache();
        } else { //SELECTION_MODE_HIDE
            imlib_context_set_color(color.red, color.green, color.blue, alpha);
            imlib_image_fill_rectangle(x, y, rect1.w, rect1.h);
        }
    }
    return capture;
}
