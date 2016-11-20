/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_PLATFORM_INTEL_H_
#define ANDROID_PLATFORM_INTEL_H_

#include "drmresources.h"
#include "platform.h"

#include <gralloc_drm_handle.h>
#include <hardware/gralloc.h>

namespace android {

class DrmResources;

class IAImporter : public Importer {
 public:
  IAImporter(DrmResources *drm);
  ~IAImporter() override;

  int Init();

  EGLImageKHR ImportImage(EGLDisplay egl_display, DrmHwcBuffer *buffer,
                          buffer_handle_t handle) override;
  int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) override;
  int ReleaseBuffer(hwc_drm_bo_t *bo) override;
  int CreateFrameBuffer(hwc_drm_bo_t *bo, uint32_t plane_type) override;

 private:
  uint32_t ConvertHalFormatToDrm(uint32_t hal_format);
  uint32_t GetFormatForFrameBuffer(uint32_t fourcc_format, uint32_t plane_type);
  DrmResources *drm_;
  const gralloc_module_t *gralloc_;
};

class IAPlanner : public Planner {
    virtual ~IAPlanner() {
    }

   protected:
    std::tuple<int, std::vector<DrmCompositionPlane>> ProvisionPlanes(
        std::map<size_t, DrmHwcLayer *> &layers, bool use_squash_fb,
        DrmCrtc *crtc, std::vector<DrmPlane *> *primary_planes,
        std::vector<DrmPlane *> *overlay_planes,
        std::vector<DrmPlane *> *cursor_planes) override;

   private:
    struct OverlayPlane {
     public:
      OverlayPlane(DrmPlane *plane, DrmHwcLayer *layer)
          : plane(plane), layer(layer) {
      }
      DrmPlane *plane;
      DrmHwcLayer *layer;
    };

    std::vector<DrmPlane *> GetUsableOverlayPlanes(
        DrmCrtc *crtc, std::vector<DrmPlane *> *overlay_planes) const;
    DrmPlane *PopUsableCursorPlane(
        DrmCrtc *crtc, std::vector<DrmPlane *> *cursor_planes) const;
    DrmPlane *PopUsablePrimaryPlane(
        DrmCrtc *crtc, std::vector<DrmPlane *> *primary_planes) const;
    bool IsPreCompositionNeeded(
        DrmPlane *target_plane, DrmCrtc *crtc, DrmHwcLayer &layer,
        const std::vector<OverlayPlane> &commit_planes) const;
    bool TestCommit(const std::vector<OverlayPlane> &commit_planes,
                    DrmCrtc *crtc) const;
};
}
#endif
