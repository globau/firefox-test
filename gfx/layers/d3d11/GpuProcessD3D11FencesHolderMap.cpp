/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpuProcessD3D11FencesHolderMap.h"

#include "mozilla/layers/FenceD3D11.h"

namespace mozilla {

namespace layers {

StaticAutoPtr<GpuProcessD3D11FencesHolderMap>
    GpuProcessD3D11FencesHolderMap::sInstance;

/* static */
void GpuProcessD3D11FencesHolderMap::Init() {
  MOZ_ASSERT(XRE_IsGPUProcess() || XRE_IsParentProcess());
  sInstance = new GpuProcessD3D11FencesHolderMap();
}

/* static */
void GpuProcessD3D11FencesHolderMap::Shutdown() {
  MOZ_ASSERT(XRE_IsGPUProcess() || XRE_IsParentProcess());
  sInstance = nullptr;
}

GpuProcessD3D11FencesHolderMap::GpuProcessD3D11FencesHolderMap()
    : mMonitor("GpuProcessD3D11FencesHolderMap::mMonitor") {}

GpuProcessD3D11FencesHolderMap::~GpuProcessD3D11FencesHolderMap() {}

void GpuProcessD3D11FencesHolderMap::Register(
    GpuProcessFencesHolderId aHolderId) {
  MonitorAutoLock lock(mMonitor);

  mFencesHolderById[aHolderId] = MakeUnique<FencesHolder>();
}
void GpuProcessD3D11FencesHolderMap::Unregister(
    GpuProcessFencesHolderId aHolderId) {
  MonitorAutoLock lock(mMonitor);

  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    return;
  }
  mFencesHolderById.erase(it);
}

void GpuProcessD3D11FencesHolderMap::SetWriteFence(
    GpuProcessFencesHolderId aHolderId, RefPtr<FenceD3D11> aWriteFence) {
  MOZ_ASSERT(aWriteFence);

  if (!aWriteFence) {
    return;
  }

  MonitorAutoLock lock(mMonitor);

  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RefPtr<FenceD3D11> fence = aWriteFence->CloneFromHandle();
  if (!fence) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  MOZ_ASSERT(!it->second->mWriteFence);
  MOZ_ASSERT(it->second->mReadFences.empty());

  it->second->mWriteFence = fence;
}

void GpuProcessD3D11FencesHolderMap::SetReadFence(
    GpuProcessFencesHolderId aHolderId, RefPtr<FenceD3D11> aReadFence) {
  MOZ_ASSERT(aReadFence);

  if (!aReadFence) {
    return;
  }

  MonitorAutoLock lock(mMonitor);

  auto it = mFencesHolderById.find(aHolderId);
  if (it == mFencesHolderById.end()) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  RefPtr<FenceD3D11> fence = aReadFence->CloneFromHandle();
  if (!fence) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }

  it->second->mReadFences.push_back(fence);
}

bool GpuProcessD3D11FencesHolderMap::WaitWriteFence(
    GpuProcessFencesHolderId aHolderId, ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return false;
  }

  RefPtr<FenceD3D11> writeFence;
  {
    MonitorAutoLock lock(mMonitor);

    auto it = mFencesHolderById.find(aHolderId);
    if (it == mFencesHolderById.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return false;
    }
    writeFence = it->second->mWriteFence;
  }

  if (!writeFence) {
    return true;
  }

  return writeFence->Wait(aDevice);
}

bool GpuProcessD3D11FencesHolderMap::WaitAllFencesAndForget(
    GpuProcessFencesHolderId aHolderId, ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return false;
  }

  RefPtr<FenceD3D11> writeFence;
  std::vector<RefPtr<FenceD3D11>> readFences;
  {
    MonitorAutoLock lock(mMonitor);

    auto it = mFencesHolderById.find(aHolderId);
    if (it == mFencesHolderById.end()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
      return false;
    }
    writeFence = it->second->mWriteFence.forget();
    readFences.swap(it->second->mReadFences);

    MOZ_ASSERT(!it->second->mWriteFence);
    MOZ_ASSERT(it->second->mReadFences.empty());
  }

  if (writeFence) {
    writeFence->Wait(aDevice);
  }

  for (auto& fence : readFences) {
    fence->Wait(aDevice);
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla
