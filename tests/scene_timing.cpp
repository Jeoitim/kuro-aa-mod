// SPDX-License-Identifier: MIT
#include "../src/SceneScaleTiming.hpp"
#include <windows.h>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
int main(int argc,char **argv){try{
    require(argc==2,"Supply a fresh output directory");
    const std::filesystem::path root=argv[1];std::filesystem::create_directories(root);
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"D3D11 device failed");
    {
        SceneScaleTiming disabled;require(disabled.open(device,root,0),"Disabled profiler failed");
        disabled.begin(context,false,1920,1080);require(!disabled.active(),"Disabled profiler sampled");
        require(!std::filesystem::exists(root/"KuroUI-scale-timing.csv"),"Disabled profiler wrote diagnostics");
    }
    {
        SceneScaleTiming timing;require(timing.open(device,root,12),"Query initialization failed");
        for(unsigned i=0;i<1000 && !timing.done();++i){
            timing.poll(context);timing.begin(context,i%2!=0,i%2?1440:1920,i%2?810:1080);
            {
                SceneScaleTiming::Frame frame{timing,context};
                {auto draw=timing.draw();auto constants=timing.constants();
                    for(int patch=0;patch<10;++patch){timing.patch_begin(context);timing.patch_end(context);}}
                frame.mark();frame.mark(); // destructor must complete remaining boundaries
                if(i%2){timing.aa_begin(context);timing.aa_end(context);}
            }
            if(i%2==0){timing.aa_begin(context);timing.aa_end(context);}
            // Flush is deliberately confined to this fixture; production polling never flushes.
            context->Flush();Sleep(1);
        }
        require(timing.done(),"Sampling did not stop at the limit");
        for(int i=0;i<100;++i){context->Flush();Sleep(1);timing.poll(context);}
    }
    std::ifstream input(root/"KuroUI-scale-timing.csv");std::string line;std::getline(input,line);unsigned rows=0;
    while(std::getline(input,line)){
        std::stringstream stream(line);std::string field;std::vector<std::string> fields;
        while(std::getline(stream,field,','))fields.push_back(field);
        require(fields.size()==26,"Wrong timing column count");require(fields[4]=="1","Disjoint or unordered GPU timestamps");
        require(fields[6]=="1" && fields[20]=="8" && fields[21]=="2","Callback counts or patch overflow incorrect");
        require(fields[22]=="1" && fields[23]=="1","AA sample missing or invalid");++rows;
    }
    require(rows==12,"Bounded asynchronous samples were lost");context->Release();device->Release();
    std::cout<<"Timing: disabled path, native/scaled metadata, 12 asynchronous samples, patch overflow, AA queries and early-boundary completion passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
