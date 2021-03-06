#pragma once

#include <torch/nn/module.h>
#include <torch/tensor.h>

#include <ATen/Error.h>

#include <memory>
#include <utility>

namespace torch {
namespace nn {
/// The `clone()` method in the base `Module` class does not have knowledge of
/// the concrete runtime type of its subclasses. Therefore, `clone()` must
/// either be called from within the subclass, or from a base class that has
/// knowledge of the concrete type. `Cloneable` uses the CRTP to gain
/// knowledge of the subclass' static type and provide an implementation of the
/// `clone()` method. We do not want to use this pattern in the base class,
/// because then storing a module would always require templatizing it.
template <typename Derived>
class Cloneable : public Module {
 public:
  using Module::Module;

  /// `reset()` must perform initialization of all members with reference
  /// semantics, most importantly parameters, buffers and submodules.
  virtual void reset() = 0;

  /// Performs a recursive "deep copy" of the `Module`, such that all parameters
  /// and submodules in the cloned module are different from those in the
  /// original module.
  std::shared_ptr<Module> clone() const override {
    const auto& self = static_cast<const Derived&>(*this);
    auto copy = std::make_shared<Derived>(self);
    copy->parameters_.clear();
    copy->buffers_.clear();
    copy->children_.clear();
    copy->reset();
    AT_CHECK(
        copy->parameters_.size() == parameters_.size(),
        "The cloned module does not have the same number of "
        "parameters as the original module after calling reset(). "
        "Are you sure you called register_parameter() inside reset() "
        "and not the constructor?");
    for (const auto& parameter : parameters_) {
      copy->parameters_[parameter.key].data().copy_(
          parameter->data(), /*non_blocking=*/true);
    }
    AT_CHECK(
        copy->buffers_.size() == buffers_.size(),
        "The cloned module does not have the same number of "
        "buffers as the original module after calling reset(). "
        "Are you sure you called register_buffer() inside reset() "
        "and not the constructor?");
    for (const auto& buffer : buffers_) {
      copy->buffers_[buffer.key].data().copy_(
          buffer->data(), /*non_blocking=*/true);
    }
    AT_CHECK(
        copy->children_.size() == children_.size(),
        "The cloned module does not have the same number of "
        "child modules as the original module after calling reset(). "
        "Are you sure you called register_module() inside reset() "
        "and not the constructor?");
    for (const auto& child : children_) {
      copy->children_[child.key]->clone_(*child.value);
    }
    return copy;
  }

 private:
  void clone_(Module& other) final override {
    // Here we are *pretty* certain that `other's` type is `Derived` (because it
    // was registered under the same name as `this`), but you never know what
    // crazy things `reset()` does, so `dynamic_cast` just to be safe.
    auto clone = std::dynamic_pointer_cast<Derived>(other.clone());
    AT_CHECK(
        clone != nullptr,
        "Attempted to clone submodule, but it is of a "
        "different type than the submodule it was to be cloned into");
    static_cast<Derived&>(*this) = std::move(*clone);
  }
};

} // namespace nn
} // namespace torch
