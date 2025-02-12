// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/display_list/display_list_vertices.h"

#include "flutter/display_list/display_list_utils.h"
#include "flutter/fml/logging.h"

namespace flutter {

using Flags = DlVertices::Builder::Flags;

static void DlVerticesDeleter(void* p) {
  // Some of our target environments would prefer a sized delete,
  // but other target environments do not have that operator.
  // Use an unsized delete until we get better agreement in the
  // environments.
  // See https://github.com/flutter/flutter/issues/100327
  ::operator delete(p);
}

static size_t bytes_needed(int vertex_count, Flags flags, int index_count) {
  int needed = sizeof(DlVertices);
  // We always have vertices
  needed += vertex_count * sizeof(SkPoint);
  if (flags.has_texture_coordinates) {
    needed += vertex_count * sizeof(SkPoint);
  }
  if (flags.has_colors) {
    needed += vertex_count * sizeof(DlColor);
  }
  if (index_count > 0) {
    needed += index_count * sizeof(uint16_t);
  }
  return needed;
}

std::shared_ptr<DlVertices> DlVertices::Make(
    DlVertexMode mode,
    int vertex_count,
    const SkPoint vertices[],
    const SkPoint texture_coordinates[],
    const DlColor colors[],
    int index_count,
    const uint16_t indices[]) {
  if (!vertices || vertex_count <= 0) {
    vertex_count = 0;
    texture_coordinates = nullptr;
    colors = nullptr;
  }
  if (!indices || index_count <= 0) {
    index_count = 0;
    indices = nullptr;
  }

  Flags flags;
  FML_DCHECK(!flags.has_texture_coordinates);
  FML_DCHECK(!flags.has_colors);
  if (texture_coordinates) {
    flags |= Builder::kHasTextureCoordinates;
  }
  if (colors) {
    flags |= Builder::kHasColors;
  }
  Builder builder(mode, vertex_count, flags, index_count);

  builder.store_vertices(vertices);
  if (texture_coordinates) {
    builder.store_texture_coordinates(texture_coordinates);
  }
  if (colors) {
    builder.store_colors(colors);
  }
  if (indices) {
    builder.store_indices(indices);
  }

  return builder.build();
}

size_t DlVertices::size() const {
  return bytes_needed(vertex_count_,
                      {{texture_coordinates_offset_ > 0, colors_offset_ > 0}},
                      index_count_);
}

static SkRect compute_bounds(const SkPoint* points, int count) {
  BoundsAccumulator accumulator;
  for (int i = 0; i < count; i++) {
    accumulator.accumulate(points[i]);
  }
  return accumulator.bounds();
}

DlVertices::DlVertices(DlVertexMode mode,
                       int unchecked_vertex_count,
                       const SkPoint* vertices,
                       const SkPoint* texture_coordinates,
                       const DlColor* colors,
                       int unchecked_index_count,
                       const uint16_t* indices,
                       const SkRect* bounds)
    : mode_(mode),
      vertex_count_(std::max(unchecked_vertex_count, 0)),
      index_count_(indices ? std::max(unchecked_index_count, 0) : 0) {
  bounds_ = bounds ? *bounds : compute_bounds(vertices, vertex_count_);

  char* pod = reinterpret_cast<char*>(this);
  size_t offset = sizeof(DlVertices);

  auto advance = [pod, &offset](auto* src, int count) {
    if (src != nullptr && count > 0) {
      size_t bytes = count * sizeof(*src);
      memcpy(pod + offset, src, bytes);
      size_t ret = offset;
      offset += bytes;
      return ret;
    } else {
      return size_t(0);
    }
  };

  vertices_offset_ = advance(vertices, vertex_count_);
  texture_coordinates_offset_ = advance(texture_coordinates, vertex_count_);
  colors_offset_ = advance(colors, vertex_count_);
  indices_offset_ = advance(indices, index_count_);
  FML_DCHECK(offset == bytes_needed(vertex_count_,
                                    {{!!texture_coordinates, !!colors}},
                                    index_count_));
}

DlVertices::DlVertices(const DlVertices* other)
    : DlVertices(other->mode_,
                 other->vertex_count_,
                 other->vertices(),
                 other->texture_coordinates(),
                 other->colors(),
                 other->index_count_,
                 other->indices(),
                 &other->bounds_) {}

DlVertices::DlVertices(DlVertexMode mode,
                       int unchecked_vertex_count,
                       Flags flags,
                       int unchecked_index_count)
    : mode_(mode),
      vertex_count_(std::max(unchecked_vertex_count, 0)),
      index_count_(std::max(unchecked_index_count, 0)) {
  char* pod = reinterpret_cast<char*>(this);
  size_t offset = sizeof(DlVertices);

  auto advance = [pod, &offset](size_t size, int count) {
    if (count > 0) {
      size_t bytes = count * size;
      memset(pod + offset, 0, bytes);
      size_t ret = offset;
      offset += bytes;
      return ret;
    } else {
      return size_t(0);
    }
  };

  vertices_offset_ = advance(sizeof(SkPoint), vertex_count_);
  texture_coordinates_offset_ = advance(
      sizeof(SkPoint), flags.has_texture_coordinates ? vertex_count_ : 0);
  colors_offset_ =
      advance(sizeof(DlColor), flags.has_colors ? vertex_count_ : 0);
  indices_offset_ = advance(sizeof(uint16_t), index_count_);
  FML_DCHECK(offset == bytes_needed(vertex_count_, flags, index_count_));
  FML_DCHECK((vertex_count_ != 0) == (vertices() != nullptr));
  FML_DCHECK((vertex_count_ != 0 && flags.has_texture_coordinates) ==
             (texture_coordinates() != nullptr));
  FML_DCHECK((vertex_count_ != 0 && flags.has_colors) == (colors() != nullptr));
  FML_DCHECK((index_count_ != 0) == (indices() != nullptr));
}

sk_sp<SkVertices> DlVertices::skia_object() const {
  const SkColor* sk_colors = reinterpret_cast<const SkColor*>(colors());
  return SkVertices::MakeCopy(ToSk(mode_), vertex_count_, vertices(),
                              texture_coordinates(), sk_colors, index_count_,
                              indices());
}

bool DlVertices::operator==(DlVertices const& other) const {
  auto lists_equal = [](auto* a, auto* b, int count) {
    if (a == nullptr || b == nullptr) {
      return a == b;
    }
    for (int i = 0; i < count; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  };
  return                                                               //
      mode_ == other.mode_ &&                                          //
      vertex_count_ == other.vertex_count_ &&                          //
      lists_equal(vertices(), other.vertices(), vertex_count_) &&      //
      lists_equal(texture_coordinates(), other.texture_coordinates(),  //
                  vertex_count_) &&                                    //
      lists_equal(colors(), other.colors(), vertex_count_) &&          //
      index_count_ == other.index_count_ &&                            //
      lists_equal(indices(), other.indices(), index_count_);
}

DlVertices::Builder::Builder(DlVertexMode mode,
                             int vertex_count,
                             Flags flags,
                             int index_count)
    : needs_vertices_(true),
      needs_texture_coords_(flags.has_texture_coordinates),
      needs_colors_(flags.has_colors),
      needs_indices_(index_count > 0) {
  vertex_count = std::max(vertex_count, 0);
  index_count = std::max(index_count, 0);
  void* storage =
      ::operator new(bytes_needed(vertex_count, flags, index_count));
  vertices_.reset(new (storage)
                      DlVertices(mode, vertex_count, flags, index_count),
                  DlVerticesDeleter);
}

static void store_points(char* dst, int offset, const float* src, int count) {
  SkPoint* points = reinterpret_cast<SkPoint*>(dst + offset);
  for (int i = 0; i < count; i++) {
    points[i] = SkPoint::Make(src[i * 2], src[i * 2 + 1]);
  }
}

void DlVertices::Builder::store_vertices(const SkPoint vertices[]) {
  FML_CHECK(is_valid());
  FML_CHECK(needs_vertices_);
  char* pod = reinterpret_cast<char*>(vertices_.get());
  size_t bytes = vertices_->vertex_count_ * sizeof(vertices[0]);
  memcpy(pod + vertices_->vertices_offset_, vertices, bytes);
  needs_vertices_ = false;
}

void DlVertices::Builder::store_vertices(const float vertices[]) {
  FML_CHECK(is_valid());
  FML_CHECK(needs_vertices_);
  char* pod = reinterpret_cast<char*>(vertices_.get());
  store_points(pod, vertices_->vertices_offset_, vertices,
               vertices_->vertex_count_);
  needs_vertices_ = false;
}

void DlVertices::Builder::store_texture_coordinates(const SkPoint coords[]) {
  FML_CHECK(is_valid());
  FML_CHECK(needs_texture_coords_);
  char* pod = reinterpret_cast<char*>(vertices_.get());
  size_t bytes = vertices_->vertex_count_ * sizeof(coords[0]);
  memcpy(pod + vertices_->texture_coordinates_offset_, coords, bytes);
  needs_texture_coords_ = false;
}

void DlVertices::Builder::store_texture_coordinates(const float coords[]) {
  FML_CHECK(is_valid());
  FML_CHECK(needs_texture_coords_);
  char* pod = reinterpret_cast<char*>(vertices_.get());
  store_points(pod, vertices_->texture_coordinates_offset_, coords,
               vertices_->vertex_count_);
  needs_texture_coords_ = false;
}

void DlVertices::Builder::store_colors(const DlColor colors[]) {
  FML_CHECK(is_valid());
  FML_CHECK(needs_colors_);
  char* pod = reinterpret_cast<char*>(vertices_.get());
  size_t bytes = vertices_->vertex_count_ * sizeof(colors[0]);
  memcpy(pod + vertices_->colors_offset_, colors, bytes);
  needs_colors_ = false;
}

void DlVertices::Builder::store_indices(const uint16_t indices[]) {
  FML_CHECK(is_valid());
  FML_CHECK(needs_indices_);
  char* pod = reinterpret_cast<char*>(vertices_.get());
  size_t bytes = vertices_->index_count_ * sizeof(indices[0]);
  memcpy(pod + vertices_->indices_offset_, indices, bytes);
  needs_indices_ = false;
}

std::shared_ptr<DlVertices> DlVertices::Builder::build() {
  FML_CHECK(is_valid());
  if (vertices_->vertex_count() <= 0) {
    // We set this to true in the constructor to make sure that they
    // call store_vertices() only once, but if there are no vertices
    // then we will not object to them never having stored any vertices
    needs_vertices_ = false;
  }
  FML_CHECK(!needs_vertices_);
  FML_CHECK(!needs_texture_coords_);
  FML_CHECK(!needs_colors_);
  FML_CHECK(!needs_indices_);

  vertices_->bounds_ =
      compute_bounds(vertices_->vertices(), vertices_->vertex_count_);

  return std::move(vertices_);
}

}  // namespace flutter
