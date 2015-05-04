//
//  test-client-cpp.cc
//  Test SceneViewer with C++ CORBA client interface.
//
//  Created by Mathieu Geisert in December 2014.
//  Copyright (c) 2014 LAAS-CNRS. All rights reserved.
//

#include <gepetto/viewer/corba/client.hh>
#include <unistd.h>

int main(int, const char **)
{
    using namespace graphics;
    using namespace corbaServer;
    using namespace std;

    ClientCpp client;

    se3::SE3 position1 = se3::SE3::Random();
    se3::SE3 position2 = se3::SE3::Random();
    se3::SE3 position3 = se3::SE3::Random();
    se3::SE3 position4 = se3::SE3::Random();
    se3::SE3 position5 = se3::SE3::Random();

    ClientCpp::WindowID windowId = client.createWindow("window1");
    client.createScene("scene1");
    client.addSceneToWindow("scene1",windowId);

    sleep(5);

    //vector<float> tri01 (3);

    float pos1[3]= {1.,0.,0.};
    float pos2[3] = {0.,1.,0.};
    float pos3[3]= {0.,1.,1.};
    float color[4] = {1.,1.,1.,1.};
    client.addTriangleFace("scene1/triangle", pos1, pos2, pos3, color);

    sleep(15);

    client.refresh();

    return 0;
}
