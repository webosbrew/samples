/* Absolute pointer positioning on webOS, headlessly.
 *
 * webOS has no absolute pointer path that is usable from outside:
 *
 *   - The com.webos.service.networkinput pointer socket is relative only. Its
 *     on-demand uinput device declares EV=7 / REL=143 and no ABS axes at all,
 *     so no wire encoding on that socket can ever place the cursor.
 *   - com.webos.service.mrcu /cursor/setPosition {posX, posY} is real and takes
 *     absolute coordinates, but answers 1301 "Magic Remote is not Ready"
 *     without a paired RF remote. A USB mouse does not satisfy it: mrcu is a
 *     sibling producer, not the cursor owner.
 *
 * The cursor owner is surface-manager, which hosts libim and reads every
 * /dev/input/event*. So the way in is to be one of those devices. Two things
 * are required together, and neither alone is enough:
 *
 *   1. The device name must be one libim recognises. Its table holds
 *      "CHECK INPUT", "LGE RCU", "LGE M-RCU - Builtin", "LGE M-RCU - For GAME",
 *      "LGE Simple Premium", "LGE Smart Remote - TouchPad" and
 *      "Smart Remote RCU Input". An arbitrary name is ignored.
 *   2. IM_RequestToCheckInput() must be called after UI_DEV_CREATE.
 *      surface-manager enumerates input devices only when asked, which is why
 *      network-input-service imports exactly that one libim symbol. Without
 *      it the node exists and is never opened.
 *
 * With both, the jump is exact and immediate - asked for (1100,620), SDL
 * reported (1100,620) - and BTN_LEFT gives a full tap.
 *
 * Coordinates are in the graphics UI resolution, not the panel: 1280x720 here,
 * per system.sysAsset. See input/pointer for how that is established.
 *
 *     ares-push -d tv abspointer /tmp/abspointer
 *     ares-shell -d tv -r "CLICK=1 /tmp/abspointer 640 360"
 *
 * Legacy uinput_user_dev interface on purpose: the TV runs 4.4.84.
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void emit(int fd, int type, int code, int val) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type; ev.code = code; ev.value = val;
    if (write(fd, &ev, sizeof(ev)) != sizeof(ev)) perror("write");
}

int main(int argc, char **argv) {
    const char *name = (argc > 3) ? argv[3] : "LGE Smart Remote - TouchPad";
    int x = (argc > 1) ? atoi(argv[1]) : 640;
    int y = (argc > 2) ? atoi(argv[2]) : 360;
    int maxx = (argc > 4) ? atoi(argv[4]) : 1280;
    int maxy = (argc > 5) ? atoi(argv[5]) : 720;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open /dev/uinput"); return 1; }

    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);

    struct uinput_user_dev dev;
    memset(&dev, 0, sizeof(dev));
    snprintf(dev.name, UINPUT_MAX_NAME_SIZE, "%s", name);
    dev.id.bustype = BUS_VIRTUAL;
    dev.id.vendor = 0x9999; dev.id.product = 0x9999; dev.id.version = 1;
    dev.absmin[ABS_X] = 0; dev.absmax[ABS_X] = maxx;
    dev.absmin[ABS_Y] = 0; dev.absmax[ABS_Y] = maxy;
    if (write(fd, &dev, sizeof(dev)) != sizeof(dev)) { perror("write dev"); return 1; }
    if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("UI_DEV_CREATE"); return 1; }
    printf("created \"%s\" abs 0..%d x 0..%d\n", name, maxx, maxy);

    /* Creating the node is not enough: LSM enumerates input devices only when
     * asked. network-input-service imports exactly one libim symbol -
     * IM_RequestToCheckInput - and that is why its device gets opened and a
     * hand-rolled one does not. */
    {
        void *im = dlopen("libim.so.1", RTLD_NOW);
        if (!im) {
            printf("dlopen libim: %s\n", dlerror());
        } else {
            int (*check)(void) = dlsym(im, "IM_RequestToCheckInput");
            printf("IM_RequestToCheckInput=%p", (void *)check);
            if (check) printf(" rc=%d", check());
            printf("\n");
        }
    }

    sleep(2);   /* let LSM notice and classify the new device */

    /* Repeated because only the first report changes anything; the rest cost
     * nothing and cover a slow enumeration. */
    for (int i = 0; i < 4; i++) {
        emit(fd, EV_ABS, ABS_X, x);
        emit(fd, EV_ABS, ABS_Y, y);
        emit(fd, EV_SYN, SYN_REPORT, 0);
        usleep(60000);
    }
    printf("moved to (%d,%d)\n", x, y);

    if (getenv("CLICK")) {
        emit(fd, EV_KEY, BTN_LEFT, 1);
        emit(fd, EV_SYN, SYN_REPORT, 0);
        usleep(80000);
        emit(fd, EV_KEY, BTN_LEFT, 0);
        emit(fd, EV_SYN, SYN_REPORT, 0);
        printf("clicked\n");
        usleep(300000);
    }
    sleep(2);

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    printf("destroyed\n");
    return 0;
}
