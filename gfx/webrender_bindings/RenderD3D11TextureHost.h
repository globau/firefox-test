/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERD3D11TEXTUREHOST_H
#define MOZILLA_GFX_RENDERD3D11TEXTUREHOST_H

#include <d3d11.h>

#include "GLTypes.h"
#include "mozilla/gfx/FileHandleWrapper.h"
#include "RenderTextureHostSWGL.h"

struct ID3D11Texture2D;
struct IDXGIKeyedMutex;

namespace mozilla {

namespace layers {
class FenceD3D11;
}  // namespace layers

namespace wr {

class RenderDXGITextureHost final : public RenderTextureHostSWGL {
 public:
  RenderDXGITextureHost(
      const RefPtr<gfx::FileHandleWrapper> aHandle,
      const Maybe<layers::GpuProcessTextureId>& aGpuProcessTextureId,
      const uint32_t aArrayIndex, const gfx::SurfaceFormat aFormat,
      const gfx::ColorSpace2 aColorSpace, const gfx::ColorRange aColorRange,
      const gfx::IntSize aSize, const bool aHasKeyedMutex,
      const Maybe<layers::GpuProcessFencesHolderId>& aFencesHolderId);

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;
  void ClearCachedResources() override;

  gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  GLuint GetGLHandle(uint8_t aChannelIndex) const;

  bool SyncObjectNeeded() override;

  RenderDXGITextureHost* AsRenderDXGITextureHost() override { return this; }

  gfx::ColorRange GetColorRange() const { return mColorRange; }

  ID3D11Texture2D* GetD3D11Texture2DWithGL();
  ID3D11Texture2D* GetD3D11Texture2D() { return mTexture; }

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override { return mFormat; }
  gfx::ColorDepth GetColorDepth() const override {
    if (mFormat == gfx::SurfaceFormat::P010) {
      return gfx::ColorDepth::COLOR_10;
    }
    if (mFormat == gfx::SurfaceFormat::P016) {
      return gfx::ColorDepth::COLOR_16;
    }
    return gfx::ColorDepth::COLOR_8;
  }
  size_t GetPlaneCount() const override;
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;
  gfx::YUVRangedColorSpace GetYUVColorSpace() const override {
    return ToYUVRangedColorSpace(ToYUVColorSpace(mColorSpace), mColorRange);
  }

  bool EnsureD3D11Texture2D(ID3D11Device* aDevice);
  bool LockInternal();

  size_t Bytes() override {
    size_t bytes = 0;

    size_t bpp = GetPlaneCount() > 1
                     ? (GetColorDepth() == gfx::ColorDepth::COLOR_8 ? 1 : 2)
                     : 4;

    for (size_t i = 0; i < GetPlaneCount(); i++) {
      gfx::IntSize size = GetSize(i);
      bytes += size.width * size.height * bpp;
    }
    return bytes;
  }

  uint32_t ArrayIndex() const { return mArrayIndex; }

  void SetIsSoftwareDecodedVideo() override { mIsSoftwareDecodedVideo = true; }
  bool IsSoftwareDecodedVideo() override { return mIsSoftwareDecodedVideo; }

 private:
  virtual ~RenderDXGITextureHost();

  bool EnsureD3D11Texture2DWithGL();
  bool EnsureLockable();

  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;

  const RefPtr<gfx::FileHandleWrapper> mHandle;
  const Maybe<layers::GpuProcessTextureId> mGpuProcessTextureId;
  RefPtr<ID3D11Texture2D> mTexture;
  const uint32_t mArrayIndex;
  RefPtr<IDXGIKeyedMutex> mKeyedMutex;

  // Temporary state between MapPlane and UnmapPlanes.
  RefPtr<ID3D11DeviceContext> mDeviceContext;
  RefPtr<ID3D11Texture2D> mCpuTexture;
  D3D11_MAPPED_SUBRESOURCE mMappedSubresource;

  EGLSurface mSurface;
  EGLStreamKHR mStream;

  // We could use NV12 format for this texture. So, we might have 2 gl texture
  // handles for Y and CbCr data.
  GLuint mTextureHandle[2];

  bool mIsSoftwareDecodedVideo = false;

 public:
  const gfx::SurfaceFormat mFormat;
  const gfx::ColorSpace2 mColorSpace;
  const gfx::ColorRange mColorRange;
  const gfx::IntSize mSize;
  const bool mHasKeyedMutex;
  const Maybe<layers::GpuProcessFencesHolderId> mFencesHolderId;

 private:
  bool mLocked;
};

class RenderDXGIYCbCrTextureHost final : public RenderTextureHostSWGL {
 public:
  explicit RenderDXGIYCbCrTextureHost(
      RefPtr<gfx::FileHandleWrapper> (&aHandles)[3],
      const gfx::YUVColorSpace aYUVColorSpace,
      const gfx::ColorDepth aColorDepth, const gfx::ColorRange aColorRange,
      const gfx::IntSize aSizeY, const gfx::IntSize aSizeCbCr,
      const layers::GpuProcessFencesHolderId aFencesHolderId);

  RenderDXGIYCbCrTextureHost* AsRenderDXGIYCbCrTextureHost() override {
    return this;
  }

  wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) override;
  void Unlock() override;
  void ClearCachedResources() override;

  gfx::IntSize GetSize(uint8_t aChannelIndex) const;
  GLuint GetGLHandle(uint8_t aChannelIndex) const;

  bool SyncObjectNeeded() override { return true; }

  gfx::ColorRange GetColorRange() const { return mColorRange; }

  // RenderTextureHostSWGL
  gfx::SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::YUV420;
  }
  gfx::ColorDepth GetColorDepth() const override { return mColorDepth; }
  size_t GetPlaneCount() const override { return 3; }
  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;
  void UnmapPlanes() override;
  gfx::YUVRangedColorSpace GetYUVColorSpace() const override {
    return ToYUVRangedColorSpace(mYUVColorSpace, GetColorRange());
  }

  bool EnsureD3D11Texture2D(ID3D11Device* aDevice);
  bool LockInternal();

  ID3D11Texture2D* GetD3D11Texture2D(uint8_t aChannelIndex) {
    return mTextures[aChannelIndex];
  }

  size_t Bytes() override {
    size_t bytes = 0;

    size_t bpp = mColorDepth == gfx::ColorDepth::COLOR_8 ? 1 : 2;

    for (size_t i = 0; i < GetPlaneCount(); i++) {
      gfx::IntSize size = GetSize(i);
      bytes += size.width * size.height * bpp;
    }
    return bytes;
  }

 private:
  virtual ~RenderDXGIYCbCrTextureHost();

  bool EnsureLockable();

  void DeleteTextureHandle();

  RefPtr<gl::GLContext> mGL;

  RefPtr<gfx::FileHandleWrapper> mHandles[3];
  RefPtr<ID3D11Texture2D> mTextures[3];
  RefPtr<ID3D11Device> mDevice;

  EGLSurface mSurfaces[3];
  EGLStreamKHR mStreams[3];

  // The gl handles for Y, Cb and Cr data.
  GLuint mTextureHandles[3];

  // Temporary state between MapPlane and UnmapPlanes.
  RefPtr<ID3D11DeviceContext> mDeviceContext;
  RefPtr<ID3D11Texture2D> mCpuTexture[3];

  const gfx::YUVColorSpace mYUVColorSpace;
  const gfx::ColorDepth mColorDepth;
  const gfx::ColorRange mColorRange;
  const gfx::IntSize mSizeY;
  const gfx::IntSize mSizeCbCr;
  const layers::GpuProcessFencesHolderId mFencesHolderId;

  bool mLocked = false;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERD3D11TEXTUREHOST_H
