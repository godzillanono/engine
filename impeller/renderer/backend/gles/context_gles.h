// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "flutter/fml/macros.h"
#include "impeller/base/backend_cast.h"
#include "impeller/renderer/backend/gles/allocator_gles.h"
#include "impeller/renderer/backend/gles/command_buffer_gles.h"
#include "impeller/renderer/backend/gles/pipeline_library_gles.h"
#include "impeller/renderer/backend/gles/reactor_gles.h"
#include "impeller/renderer/backend/gles/sampler_library_gles.h"
#include "impeller/renderer/backend/gles/shader_library_gles.h"
#include "impeller/renderer/context.h"

namespace impeller {

class ContextGLES final : public Context,
                          public BackendCast<ContextGLES, Context> {
 public:
  static std::shared_ptr<Context> Create(
      std::unique_ptr<ProcTableGLES> gl,
      std::vector<std::shared_ptr<fml::Mapping>> shader_libraries);

  // |Context|
  ~ContextGLES() override;

  const ReactorGLES::Ref GetReactor() const;

 private:
  ReactorGLES::Ref reactor_;
  std::shared_ptr<ShaderLibraryGLES> shader_library_;
  std::shared_ptr<PipelineLibraryGLES> pipeline_library_;
  std::shared_ptr<SamplerLibraryGLES> sampler_library_;
  std::shared_ptr<AllocatorGLES> permanents_allocator_;
  std::shared_ptr<AllocatorGLES> transients_allocator_;
  bool is_valid_ = false;

  ContextGLES(std::unique_ptr<ProcTableGLES> gl,
              std::vector<std::shared_ptr<fml::Mapping>> shader_libraries);

  // |Context|
  bool IsValid() const override;

  // |Context|
  std::shared_ptr<Allocator> GetPermanentsAllocator() const override;

  // |Context|
  std::shared_ptr<Allocator> GetTransientsAllocator() const override;

  // |Context|
  std::shared_ptr<ShaderLibrary> GetShaderLibrary() const override;

  // |Context|
  std::shared_ptr<SamplerLibrary> GetSamplerLibrary() const override;

  // |Context|
  std::shared_ptr<PipelineLibrary> GetPipelineLibrary() const override;

  // |Context|
  std::shared_ptr<CommandBuffer> CreateRenderCommandBuffer() const override;

  // |Context|
  std::shared_ptr<CommandBuffer> CreateTransferCommandBuffer() const override;

  FML_DISALLOW_COPY_AND_ASSIGN(ContextGLES);
};

}  // namespace impeller
