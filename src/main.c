#include "control.h"
#include "filemap.h"

int main(int argc, char *argv[]) {
    struct control *ctrl;

    ctrl = (struct control *)init_ctrl(argc, argv);

    if (!ctrl) {
        return 1;
    }

    // TODO: depending on flags load different engines
    filemap(ctrl);
    return 0;
}
