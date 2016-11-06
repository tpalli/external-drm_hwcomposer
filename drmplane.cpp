/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "hwc-drm-plane"

#include "drmplane.h"
#include "drmresources.h"

#include <cinttypes>
#include <errno.h>
#include <stdint.h>

#include <cutils/log.h>
#include <xf86drmMode.h>

namespace android {

DrmPlane::DrmPlane(DrmResources *drm, drmModePlanePtr p)
    : drm_(drm), id_(p->plane_id), possible_crtc_mask_(p->possible_crtcs) {
}

int DrmPlane::Init() {
  DrmProperty p;

  int ret = drm_->GetPlaneProperty(*this, "type", &p);
  if (ret) {
    ALOGE("Could not get plane type property");
    return ret;
  }

  uint64_t type;
  ret = p.value(&type);
  if (ret) {
    ALOGE("Failed to get plane type property value");
    return ret;
  }
  switch (type) {
    case DRM_PLANE_TYPE_OVERLAY:
    case DRM_PLANE_TYPE_PRIMARY:
    case DRM_PLANE_TYPE_CURSOR:
      type_ = (uint32_t)type;
      break;
    default:
      ALOGE("Invalid plane type %" PRIu64, type);
      return -EINVAL;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_ID", &crtc_property_);
  if (ret) {
    ALOGE("Could not get CRTC_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "FB_ID", &fb_property_);
  if (ret) {
    ALOGE("Could not get FB_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_X", &crtc_x_property_);
  if (ret) {
    ALOGE("Could not get CRTC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_Y", &crtc_y_property_);
  if (ret) {
    ALOGE("Could not get CRTC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_W", &crtc_w_property_);
  if (ret) {
    ALOGE("Could not get CRTC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_H", &crtc_h_property_);
  if (ret) {
    ALOGE("Could not get CRTC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_X", &src_x_property_);
  if (ret) {
    ALOGE("Could not get SRC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_Y", &src_y_property_);
  if (ret) {
    ALOGE("Could not get SRC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_W", &src_w_property_);
  if (ret) {
    ALOGE("Could not get SRC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_H", &src_h_property_);
  if (ret) {
    ALOGE("Could not get SRC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "rotation", &rotation_property_);
  if (ret)
    ALOGE("Could not get rotation property");

  ret = drm_->GetPlaneProperty(*this, "alpha", &alpha_property_);
  if (ret)
    ALOGI("Could not get alpha property");

  ret = drm_->GetPlaneProperty(*this, "IN_FENCE_FD", &in_fence_fd_property_);
  if (ret)
    ALOGI("Could not get IN_FENCE_FD property");

  return 0;
}

int DrmPlane::UpdateProperties(drmModeAtomicReqPtr property_set,
                               uint32_t crtc_id,
                               const DrmHwcLayer &layer) const {
  uint64_t alpha = 0xFF;
  const DrmHwcRect<int> &display_frame = layer.display_frame;
  const DrmHwcRect<float> &source_crop = layer.source_crop;
  uint64_t transform = layer.transform;
  int fence = layer.acquire_fence.get();
  if (layer.blending == DrmHwcBlending::kPreMult)
    alpha = layer.alpha;

  if (alpha != 0xFF && alpha_property_.id() == 0)
    ALOGE("Alpha is not supported on plane %d", id_);

  uint64_t rotation = 0;
  if (transform & DrmHwcTransform::kFlipH)
    rotation |= 1 << DRM_REFLECT_X;
  if (transform & DrmHwcTransform::kFlipV)
    rotation |= 1 << DRM_REFLECT_Y;
  if (transform & DrmHwcTransform::kRotate90)
    rotation |= 1 << DRM_ROTATE_90;
  else if (transform & DrmHwcTransform::kRotate180)
    rotation |= 1 << DRM_ROTATE_180;
  else if (transform & DrmHwcTransform::kRotate270)
    rotation |= 1 << DRM_ROTATE_270;
  else
    rotation |= 1 << DRM_ROTATE_0;

  if (rotation && rotation_property_.id() == 0)
    ALOGE("Rotation is not supported on plane %d", id_);

  int success = drmModeAtomicAddProperty(property_set, id_, crtc_property_.id(),
                                         crtc_id) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, fb_property_.id(),
                                      layer.buffer->fb_id) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, crtc_x_property_.id(),
                                      display_frame.left) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, crtc_y_property_.id(),
                                      display_frame.top) < 0;
  if (type_ == DRM_PLANE_TYPE_CURSOR) {
    success |=
        drmModeAtomicAddProperty(property_set, id_, crtc_w_property_.id(),
                                 layer.buffer->width) < 0;
    success |=
        drmModeAtomicAddProperty(property_set, id_, crtc_h_property_.id(),
                                 layer.buffer->height) < 0;
  } else {
    success |=
        drmModeAtomicAddProperty(property_set, id_, crtc_w_property_.id(),
                                 display_frame.right - display_frame.left) < 0;
    success |=
        drmModeAtomicAddProperty(property_set, id_, crtc_h_property_.id(),
                                 display_frame.bottom - display_frame.top) < 0;
  }

  success |= drmModeAtomicAddProperty(property_set, id_, src_x_property_.id(),
                                      (int)(source_crop.left) << 16) < 0;
  success |= drmModeAtomicAddProperty(property_set, id_, src_y_property_.id(),
                                      (int)(source_crop.top) << 16) < 0;
  if (type_ == DRM_PLANE_TYPE_CURSOR) {
    success |= drmModeAtomicAddProperty(property_set, id_, src_w_property_.id(),
                                        layer.buffer->width << 16) < 0;
    success |= drmModeAtomicAddProperty(property_set, id_, src_h_property_.id(),
                                        layer.buffer->height << 16) < 0;
  } else {
    success |= drmModeAtomicAddProperty(
                   property_set, id_, src_w_property_.id(),
                   (int)(source_crop.right - source_crop.left) << 16) < 0;
    success |= drmModeAtomicAddProperty(
                   property_set, id_, src_h_property_.id(),
                   (int)(source_crop.bottom - source_crop.top) << 16) < 0;
  }

  if (rotation_property_.id()) {
    success = drmModeAtomicAddProperty(property_set, id_,
                                       rotation_property_.id(), rotation) < 0;
  }

  if (alpha_property_.id()) {
    success = drmModeAtomicAddProperty(property_set, id_, alpha_property_.id(),
                                       alpha) < 0;
  }

  if (fence != -1 && in_fence_fd_property_.id()) {
     success = drmModeAtomicAddProperty(property_set, id_,
                                        in_fence_fd_property_.id(), fence) < 0;
  }

  if (success) {
    ALOGE("Could not update properties for plane with id: %d", id_);
    return -EINVAL;
  }

  return success;
}

int DrmPlane::Disable(drmModeAtomicReqPtr property_set) const {
  int success =
      drmModeAtomicAddProperty(property_set, id_, crtc_property_.id(), 0) < 0;
  success |=
      drmModeAtomicAddProperty(property_set, id_, fb_property_.id(), 0) < 0;

  if (success) {
    ALOGE("Failed to disable plane with id: %d", id_);
    return -EINVAL;
  }

  return success;
}

uint32_t DrmPlane::id() const {
  return id_;
}

bool DrmPlane::GetCrtcSupported(const DrmCrtc &crtc) const {
  return !!((1 << crtc.pipe()) & possible_crtc_mask_);
}

uint32_t DrmPlane::type() const {
  return type_;
}

const DrmProperty &DrmPlane::crtc_property() const {
  return crtc_property_;
}

const DrmProperty &DrmPlane::fb_property() const {
  return fb_property_;
}

const DrmProperty &DrmPlane::crtc_x_property() const {
  return crtc_x_property_;
}

const DrmProperty &DrmPlane::crtc_y_property() const {
  return crtc_y_property_;
}

const DrmProperty &DrmPlane::crtc_w_property() const {
  return crtc_w_property_;
}

const DrmProperty &DrmPlane::crtc_h_property() const {
  return crtc_h_property_;
}

const DrmProperty &DrmPlane::src_x_property() const {
  return src_x_property_;
}

const DrmProperty &DrmPlane::src_y_property() const {
  return src_y_property_;
}

const DrmProperty &DrmPlane::src_w_property() const {
  return src_w_property_;
}

const DrmProperty &DrmPlane::src_h_property() const {
  return src_h_property_;
}

const DrmProperty &DrmPlane::rotation_property() const {
  return rotation_property_;
}

const DrmProperty &DrmPlane::alpha_property() const {
  return alpha_property_;
}

const DrmProperty &DrmPlane::in_fence_fd_property() const {
  return in_fence_fd_property_;
}
}
