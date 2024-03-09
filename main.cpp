#include <X11/Xlib.h>
#include <cstdio>
#include <cstdlib>
#include <X11/extensions/XInput2.h>

int dead_regions[][4] = {
        // WxH+x+y
        {2560 + 10, 2160 - 1440, 3840, 0},
};
int monitors[][6] = {
        // WxH+x+y mm x mm
        {3840, 2160, 0, 0, 697, 392}, // HDMI-0
        {2560, 1440, 3850, 720, 600, 340}, // DP-2
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
[[noreturn]] int main() {
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    Window root = DefaultRootWindow(display);

    int xi_opcode, event, error;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
        fprintf(stderr, "Error: XInput extension is not supported!\n");
        exit(1);
    }

    int major = 2;
    int minor = 0;
    if (XIQueryVersion(display, &major, &minor) != Success) {
        fprintf(stderr, "Error: XInput 2.0 is not supported (ancient X11?)\n");
        exit(1);
    }

    unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8] = {0}; // must be zeroed!
    XISetMask(mask_bytes, XI_RawMotion);

    // Set mask to receive events from all master devices
    XIEventMask evmasks[1];
    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask_bytes);
    evmasks[0].mask = mask_bytes;
    XISelectEvents(display, root, evmasks, 1);

    XEvent xevent;

    Window root_return, child_return;
    unsigned int mask_return;
    int win_x, win_y;

    int px, py;
    int cx, cy;
    int nx, ny;
    int wasInMonitor = -1;

    while (true) {
        XNextEvent(display, &xevent);
        if (xevent.xcookie.type != GenericEvent || xevent.xcookie.extension != xi_opcode) {
            continue;
        }
        XGetEventData(display, &xevent.xcookie);
        if (xevent.xcookie.evtype != XI_RawMotion) {
            XFreeEventData(display, &xevent.xcookie);
            continue;
        }
        XFreeEventData(display, &xevent.xcookie);

        bool shouldCorrect = false;

        if (!XQueryPointer(display, root, &root_return, &child_return, &cx, &cy, &win_x, &win_y, &mask_return)) {
            continue;
        }

        nx = cx;
        ny = cy;

        // monitor gap adjustment
        int isInMonitor = -1;
        int monitorCount = sizeof(monitors) / sizeof(monitors[0]);

        for (int monitor = 0; monitor < monitorCount; ++monitor) {
            int mx = monitors[monitor][2], my = monitors[monitor][3];
            int mw = monitors[monitor][0], mh = monitors[monitor][1];

            if (mx <= cx && cx <= mx + mw && my <= cy && cy <= my + mh) {
                isInMonitor = monitor;
            }
        }

        // hardcoded checks
        if (wasInMonitor != isInMonitor) {
            if (wasInMonitor == 0) { // was in HDMI-0
                // only direction of exit is right
                // so no need to check direction, only y coordinate

                // not the full formula
                // pxhA - mmhB * pxhA  / mmhA
                int xx = monitors[0][1] - monitors[1][5] * monitors[0][1] / monitors[0][5];
                if (cy > xx) {
                    // A:B -> C:D    ((x - A) / (B - A)) * (D - C) + C
                    // xx:hA -> xB:xB+hB
                    // ((cy - xx) * hB / (hA - xx)) + yB
                    ny = (cy - xx) * monitors[1][1] / (monitors[0][1] - xx) + monitors[1][3];
                    shouldCorrect = true;
                }
            }

            if (wasInMonitor == 1) { // was in DP-2
                // check to see if exit was to the left
                // bx <= cx && cx <= bx + bw
                if (monitors[1][3] <= cy && cy <= monitors[1][3] + monitors[1][1]) {
                    // not possible to move to the right, no check needed

                    // not possible to move left into nothiness, no check needed

                    // xB:xB+hB -> xx:hA
                    // (cy - xB) * (hA - xx) / hB + xx
                    int xx = monitors[0][1] - monitors[1][5] * monitors[0][1] / monitors[0][5];
                    ny = (cy - monitors[1][3]) * (monitors[0][1] - xx) / monitors[1][1] + xx;
                    if (nx >= monitors[0][0]) {
                        nx = monitors[0][0] - 1;
                    }
                    shouldCorrect = true;
                }
            }
        }
        wasInMonitor = isInMonitor;

        // dead region avoidance
        for (const auto &region: dead_regions) {
            if (shouldCorrect) {
                break;
            }

            int bx = region[2], by = region[3], bw = region[0], bh = region[1];

            bool c_region_x = bx <= cx && cx <= bx + bw;
            bool c_region_y = by <= cy && cy <= by + bh;
            bool p_region_x = bx <= px && px <= bx + bw;
            bool p_region_y = by <= py && py <= by + bh;

            if (c_region_x && c_region_y) {
                // mouse is currently in the region
                shouldCorrect = true;

                if (p_region_x) { // vertical
                    if (py >= by) {
                        ny = by + bh + 1; // to bottom
                    } else {
                        ny = by - 1; // to top
                    }
                }
                if (p_region_y) { // horizontal
                    if (px >= bx) {
                        nx = bx + bw + 1; // to right
                    } else {
                        nx = bx - 1; // to left
                    }
                }

                break; // exit on 1st found region
            }
        }

        if (shouldCorrect) {
            XWarpPointer(display, None, None, 0, 0, 0, 0, nx - cx, ny - cy);
            px = nx;
            py = ny;
        } else {
            px = cx;
            py = cy;
        }
    }
}
#pragma clang diagnostic pop
