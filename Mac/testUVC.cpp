#include "CameraDevice.h"
#include <stdio.h>
#include <memory>

int main(int argc, char ** argv)
{
    CameraQueryInterface q;

    std::vector<std::string> devPaths;
    if (!q.getAllJabraDevices(devPaths)) {
        printf("hurr\n");
        return -1;
    }
    

    for (unsigned k=0;k<devPaths.size();k++) {
        printf("%d: %s\n", k, devPaths[k].c_str());
    }

    std::shared_ptr<CameraDeviceInterface> camera;
    camera.reset(q.openJabraDevice(devPaths[0]));
    
    if (!camera) {
        printf("This camera aint workin\n");
        return -1;
    }
    
    PropertyType t = Contrast;
    Property p(0, 0, 0);
    if (!camera->getProperty(t, p)) {
        printf("This camera\'s brightness aint workin\n");
        return -1;
    }
    printf("Got brightness yo: current: %d, min: %d, max: %d\n", p.value, p.min, p.max);
    p.value += 20;
    if (!camera->setProperty(t, p.value)){
        printf("This camera\'s brightness set function aint workin\n");
        return -1;
    }
    if (!camera->getProperty(t, p)) {
        printf("This camera\'s brightness aint workin\n");
        return -1;
    }
    printf("Got brightness yo: current: %d, min: %d, max: %d\n", p.value, p.min, p.max);

    return 0;

}

