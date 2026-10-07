// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstddef>

struct EngineTargetPoolRecord {
    void *target;
    uint32_t format,width,height;
    uint8_t active,flag;
    uint16_t reserved;
};
static_assert(sizeof(EngineTargetPoolRecord)==0x18 && offsetof(EngineTargetPoolRecord,active)==0x14,"CLE target pool layout");

// The renderer and its UI share these two slots. Restore them even on exceptions.
class ScopedSceneTargets {
    void **color_,**depth_;
    void *original_color_,*original_depth_;
    void **normal_;
    void *original_normal_;
public:
    ScopedSceneTargets(void **color,void **depth,void *scene_color,void *scene_depth,void **normal=nullptr,void *scene_normal=nullptr)
        :color_(color),depth_(depth),original_color_(*color),original_depth_(*depth),normal_(normal),original_normal_(normal?*normal:nullptr){
        *color_=scene_color;*depth_=scene_depth;
        if(normal_)*normal_=scene_normal;
    }
    ScopedSceneTargets(const ScopedSceneTargets&)=delete;
    ~ScopedSceneTargets(){*color_=original_color_;*depth_=original_depth_;if(normal_)*normal_=original_normal_;}
};
