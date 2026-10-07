// SPDX-License-Identifier: MIT
#include "../src/ScopedSceneTargets.hpp"
#include <stdexcept>
#include <iostream>
int main(){
    int full_color=0,full_depth=0,low_color=0,low_depth=0;
    void *color=&full_color,*depth=&full_depth;
    int full_normal=0,low_normal=0;void *normal=&full_normal;
    EngineTargetPoolRecord color_record{&low_color,5,1440,810,1,0,0},depth_record{&low_depth,7,1440,810,1,0,0};
    for(int i=0;i<1000;++i){
        try{
            ScopedSceneTargets scope(&color,&depth,color_record.target,depth_record.target,&normal,&low_normal);
            if(color!=&low_color || depth!=&low_depth)return 1;
            if(color==&color_record || depth==&depth_record)return 3;
            if(normal!=&low_normal)return 4;
            if(i%2)throw std::runtime_error("fixture unwind");
        }catch(const std::runtime_error&){}
        if(color!=&full_color || depth!=&full_depth || normal!=&full_normal)return 2;
    }
    std::cout<<"Scoped scene targets: 1000 calls and exception restoration passed\n";
}
