#include "CameraDevice.h"
#include <stdio.h>
#include <memory>

int main(int argc, char ** argv)
{
    CameraQueryInterface q;

    std::vector<DeviceProperty> devPaths;
    if (!q.getAllJabraDevices(devPaths)) {
        printf("hurr\n");
        return -1;
    }

    for (unsigned k=0;k<devPaths.size();k++) {
        printf("%d: %s\n", k, devPaths[k].deviceName.c_str());
    }

    return 0;

}

