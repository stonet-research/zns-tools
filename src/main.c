#include "filemap.h"

int main(int argc, char *argv[]) {

    init_ctrl(argc, argv);

    // TODO: depending on flags load different engines
    filemap();

    return 0;
}
