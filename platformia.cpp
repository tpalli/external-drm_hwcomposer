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

#define LOG_TAG "hwc-platform-IA"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>

#include "platformia.h"

namespace android {

#define ALIGN(val, align) (((val) + (align)-1) & ~((align)-1))

#ifdef USE_IA_PLANNER
// static
Importer *Importer::CreateInstance(DrmResources *drm) {
  IAImporter *importer = new IAImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the IA importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}
#endif

IAImporter::IAImporter(DrmResources *drm) : drm_(drm) {
}

IAImporter::~IAImporter() {
}

int IAImporter::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module");
    return ret;
  }
  return 0;
}

uint32_t IAImporter::GetFormatForFrameBuffer(uint32_t fourcc_format,
                                             uint32_t plane_type) {
  if (plane_type != DRM_PLANE_TYPE_PRIMARY)
    return fourcc_format;

  // We only support 24 bit colordepth for primary planes on
  // pre SKL Hardware. Ideally, we query format support from
  // plane to determine this.
  switch (fourcc_format) {
    case DRM_FORMAT_ABGR8888:
      return DRM_FORMAT_XBGR8888;
    case DRM_FORMAT_ARGB8888:
      return DRM_FORMAT_XRGB8888;
    default:
      break;
  }

  return fourcc_format;
}

EGLImageKHR IAImporter::ImportImage(EGLDisplay egl_display,
                                    DrmHwcBuffer *buffer,
                                    buffer_handle_t handle) {
  gralloc_drm_handle_t *gr_handle = gralloc_drm_handle(handle);
  EGLImageKHR image = EGL_NO_IMAGE_KHR;
  // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
  // target, the EGL will take a reference to the dma_buf.
  if (buffer->operator->()->format == DRM_FORMAT_YUV420) {
    const EGLint attr_list_yv12[] = {
	EGL_WIDTH,
	static_cast<EGLint>(buffer->operator->()->width),
	EGL_HEIGHT,
	static_cast<EGLint>(buffer->operator->()->height),
	EGL_LINUX_DRM_FOURCC_EXT,
	DRM_FORMAT_YUV420,
	EGL_DMA_BUF_PLANE0_FD_EXT,
	gr_handle->prime_fd,
	EGL_DMA_BUF_PLANE0_PITCH_EXT,
	static_cast<EGLint>(buffer->operator->()->pitches[0]),
	EGL_DMA_BUF_PLANE0_OFFSET_EXT,
	static_cast<EGLint>(buffer->operator->()->offsets[0]),
	EGL_DMA_BUF_PLANE1_FD_EXT,
	gr_handle->prime_fd,
	EGL_DMA_BUF_PLANE1_PITCH_EXT,
	static_cast<EGLint>(buffer->operator->()->pitches[1]),
	EGL_DMA_BUF_PLANE1_OFFSET_EXT,
	static_cast<EGLint>(buffer->operator->()->offsets[1]),
	EGL_DMA_BUF_PLANE2_FD_EXT,
	gr_handle->prime_fd,
	EGL_DMA_BUF_PLANE2_PITCH_EXT,
	static_cast<EGLint>(buffer->operator->()->pitches[2]),
	EGL_DMA_BUF_PLANE2_OFFSET_EXT,
	static_cast<EGLint>(buffer->operator->()->offsets[2]),
	EGL_NONE,
	0};
    image = eglCreateImageKHR(
	egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
	static_cast<EGLClientBuffer>(nullptr), attr_list_yv12);
  } else {
    const EGLint attr_list[] = {
	EGL_WIDTH,
	static_cast<EGLint>(buffer->operator->()->width),
	EGL_HEIGHT,
	static_cast<EGLint>(buffer->operator->()->height),
	EGL_LINUX_DRM_FOURCC_EXT,
	static_cast<EGLint>(buffer->operator->()->format),
	EGL_DMA_BUF_PLANE0_FD_EXT,
	gr_handle->prime_fd,
	EGL_DMA_BUF_PLANE0_PITCH_EXT,
	static_cast<EGLint>(buffer->operator->()->pitches[0]),
	EGL_DMA_BUF_PLANE0_OFFSET_EXT,
	0,
	EGL_NONE,
	0};
    image =
	eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
			  static_cast<EGLClientBuffer>(nullptr), attr_list);
  }

  return image;
}

int IAImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {
  int ret = gralloc_->perform(gralloc_, GRALLOC_MODULE_PERFORM_DRM_IMPORT,
                              drm_->fd(), handle, bo);
  if (ret)
    ALOGE("GRALLOC_MODULE_PERFORM_DRM_IMPORT failed %d", ret);

  return ret;
}

int IAImporter::CreateFrameBuffer(hwc_drm_bo_t *bo, uint32_t plane_type) {
  uint32_t format = GetFormatForFrameBuffer(bo->format, plane_type);
  int ret =
      drmModeAddFB2(drm_->fd(), bo->width, bo->height, format, bo->gem_handles,
                    bo->pitches, bo->offsets, &bo->fb_id, 0);

  if (ret) {
    ALOGE("drmModeAddFB2 error (%dx%d, %c%c%c%c, handle %d pitch %d) (%s)",
          bo->width, bo->height, format, format >> 8, format >> 16,
          format >> 24, bo->gem_handles[0], bo->pitches[0], strerror(-ret));
  }

  return ret;
}

int IAImporter::ReleaseBuffer(hwc_drm_bo_t *bo) {
  if (bo->fb_id && drmModeRmFB(drm_->fd(), bo->fb_id))
    ALOGE("Failed to remove fb");

  return 0;
}

std::vector<DrmPlane *> IAPlanner::GetUsableOverlayPlanes(
    DrmCrtc *crtc, std::vector<DrmPlane *> *overlay_planes) const {
  std::vector<DrmPlane *> usable_planes;
  std::copy_if(overlay_planes->begin(), overlay_planes->end(),
	       std::back_inserter(usable_planes),
	       [=](DrmPlane *plane) { return plane->GetCrtcSupported(*crtc); });
  return usable_planes;
}

DrmPlane *IAPlanner::PopUsableCursorPlane(
    DrmCrtc *crtc, std::vector<DrmPlane *> *cursor_planes) const {
  DrmPlane *cursor_plane = NULL;
  for (auto i = cursor_planes->begin(); i != cursor_planes->end(); ++i) {
    if ((*i)->GetCrtcSupported(*crtc)) {
      cursor_plane = *i;
      cursor_planes->erase(i);
      break;
    }
  }

  return cursor_plane;
}

DrmPlane *IAPlanner::PopUsablePrimaryPlane(
    DrmCrtc *crtc, std::vector<DrmPlane *> *primary_planes) const {
  DrmPlane *primary_plane = NULL;
  for (auto i = primary_planes->begin(); i != primary_planes->end(); ++i) {
    if ((*i)->GetCrtcSupported(*crtc)) {
      primary_plane = *i;
      primary_planes->erase(i);
      break;
    }
  }

  return primary_plane;
}

std::tuple<int, std::vector<DrmCompositionPlane>> IAPlanner::ProvisionPlanes(
    std::map<size_t, DrmHwcLayer *> &layers, bool /*use_squash_fb*/,
    DrmCrtc *crtc, std::vector<DrmPlane *> *primary_planes,
    std::vector<DrmPlane *> *overlay_planes,
    std::vector<DrmPlane *> *cursor_planes) {
  std::vector<DrmCompositionPlane> composition;
  DrmPlane *next_plane = NULL;
  DrmPlane *current_plane = NULL;
  std::vector<OverlayPlane> commit_planes;
  std::vector<size_t> source_layers;
  DrmHwcLayer *cursor_layer = NULL;
  size_t cursor_index = 0;

  // We start off with Primary plane.
  current_plane = PopUsablePrimaryPlane(crtc, primary_planes);

  if (!current_plane)
    return std::make_tuple(-ENODEV, std::vector<DrmCompositionPlane>());

  // Retrieve cursor layer data and delete it from the layers.
  for (auto j = layers.rbegin(); j != layers.rend(); ++j) {
    if (j->second->gralloc_buffer_usage & GRALLOC_USAGE_CURSOR) {
      cursor_index = j->first;
      cursor_layer = j->second;
      layers.erase(std::next(j).base());
      break;
    }
  }

  if (layers.empty())
    return std::make_tuple(-ENODEV, std::vector<DrmCompositionPlane>());

  commit_planes.push_back(OverlayPlane(current_plane, layers.begin()->second));

  bool force_pre_comp = false;
  // Lets ensure we fall back to GPU composition in case
  // primary layer cannot be scanned out directly.
  if (IsPreCompositionNeeded(current_plane, crtc, *(layers.begin()->second),
			     commit_planes)) {
    force_pre_comp = true;
  }

  source_layers.emplace_back(layers.begin()->first);
  layers.erase(layers.begin());

  if (!layers.empty()) {
    std::vector<DrmPlane *> overlays =
	GetUsableOverlayPlanes(crtc, overlay_planes);

    if (!overlays.empty()) {
      // Handle remaining overlay planes.
      for (auto i = layers.begin(); i != layers.end();) {
	DrmHwcLayer &layer = *(i->second);
	if (!next_plane) {
	  next_plane = *(overlays.begin());
	  commit_planes.emplace_back(OverlayPlane(next_plane, i->second));
	} else {
	  commit_planes.back().layer = i->second;
	}
	// If we are able to composite buffer with the given plane, lets use
	// it.
	if (!IsPreCompositionNeeded(next_plane, crtc, layer, commit_planes)) {
	  DrmCompositionPlane::Type type = DrmCompositionPlane::Type::kLayer;
	  if (source_layers.size() > 1 || force_pre_comp) {
	    type = DrmCompositionPlane::Type::kPrecomp;
	    force_pre_comp = false;
	  }

	  composition.emplace_back(type, current_plane, crtc, source_layers);

	  overlays.erase(overlays.begin());
	  current_plane = next_plane;
	  next_plane = NULL;
	  if (overlays.empty())
	    break;
	}

	source_layers.emplace_back(i->first);
	i = layers.erase(i);
      }

      // Ensure any unused overlays get disabled.
      for (auto n = overlays.begin(); n != overlays.end();
	   n = overlays.erase(n)) {
	overlay_planes->emplace_back(*n);
      }
    }
  }

  // We dont have any additional planes. Pre composite remaining layers
  // to the last overlay plane.
  for (auto i = layers.begin(); i != layers.end(); i++) {
    source_layers.emplace_back(i->first);
  }

  DrmPlane *cursor_plane = NULL;
  if (cursor_layer) {
    // Handle Cursor layer. If we have dedicated cursor plane, try using it
    // to composite cursor layer.
    cursor_plane = PopUsableCursorPlane(crtc, cursor_planes);
    if (cursor_plane) {
      commit_planes.insert(commit_planes.begin(),
			   OverlayPlane(cursor_plane, cursor_layer));
      // Lets ensure we fall back to GPU composition in case
      // cursor layer cannot be scanned out directly.
      if (IsPreCompositionNeeded(cursor_plane, crtc, *(cursor_layer),
				 commit_planes)) {
	  cursor_planes->emplace_back(cursor_plane);
	  cursor_plane = NULL;
      }
    }

    if (!cursor_plane)
      source_layers.emplace_back(cursor_index);
  }

  if (source_layers.size()) {
    DrmCompositionPlane::Type comp_type = DrmCompositionPlane::Type::kLayer;
    if (source_layers.size() > 1 || force_pre_comp)
      comp_type = DrmCompositionPlane::Type::kPrecomp;

    composition.emplace_back(comp_type, current_plane, crtc, source_layers);
  }

  if (cursor_plane)
    composition.emplace_back(DrmCompositionPlane::Type::kLayer, cursor_plane, crtc, cursor_index);

  return std::make_tuple(0, std::move(composition));
}

bool IAPlanner::IsPreCompositionNeeded(
    DrmPlane *target_plane, DrmCrtc *crtc, DrmHwcLayer &layer,
    const std::vector<OverlayPlane> &commit_planes) const {
  if (!target_plane->CanCompositeLayer(layer))
    return true;

  if ((layer.buffer->fb_id <= 0) &&
      layer.buffer.CreateFrameBuffer(target_plane->type()))
    return true;

  // TODO(kalyank): Take relevant factors into consideration to determine if
  // Plane Composition makes sense. i.e. layer size etc

  if (!TestCommit(commit_planes, crtc))
    return true;

  return false;
}

bool IAPlanner::TestCommit(const std::vector<OverlayPlane> &commit_planes,
			   DrmCrtc *crtc) const {
  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  DrmResources *drm = crtc->drm_resources();
  for (auto i = commit_planes.begin(); i != commit_planes.end(); i++) {
    if (i->plane->UpdateProperties(pset, crtc->id(), *(i->layer))) {
      ALOGE("Failed to update Plane.");
      return false;
    }
  }

  if (drmModeAtomicCommit(drm->fd(), pset, DRM_MODE_ATOMIC_TEST_ONLY, drm))
    return false;

  return true;
}

#ifdef USE_IA_PLANNER
std::unique_ptr<Planner> Planner::CreateInstance(DrmResources *) {
  std::unique_ptr<Planner> planner(new IAPlanner);
  return planner;
}
#endif
}
