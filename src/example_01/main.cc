// Copyright 2022 The Dusk Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <print>
#include <cstring>
#include <iostream>

#include "src/common/callback.h"
#include "src/common/expected.h"
#include "src/common/log.h"
#include "src/common/wgpu.h"

int main(int argc, const char** argv) {

  bool try_f16 = false;
  for (int i = 0; i < argc; i++) {
    if (0 == strcmp(argv[i],"-f16")) {
      try_f16 = true;
    }
  }
  std::cout << "Trying to enable shader-f16? " << (try_f16 ? "yes" : "no") << std::endl;

  // Get the default instance.
  //
  // Alternatively, you can use an InstanceDescriptor to request an instance
  // with specific features (see InstanceFeatureName), minimum limits
  // (InstanceLimits), or another feature based on a chained struct (see "Can be
  // chained in InstanceDescriptor" in webgpu_cpp.h header).
  auto instance = wgpu::CreateInstance();
  dusk::valid_or_exit(dusk::log::emit_instance_language_features(instance));

  // Use an Adapter toggle to expose shader-f16 in NV devices when
  // the relevant Vulkan features are supported. This overrides the
  // default Dawn denylist. The feature is disabled by default because
  // NV devices fail some WebGPU conformance tests for f16.
  std::vector<const char*> adapter_toggles;
  adapter_toggles.push_back("vulkan_enable_f16_on_nvidia");
  wgpu::DawnTogglesDescriptor desc;
  desc.enabledToggles = adapter_toggles.data();
  desc.enabledToggleCount = adapter_toggles.size();

  // Get Adapter
  wgpu::RequestAdapterOptions adapter_opts{
      .nextInChain = &desc,
      .powerPreference = wgpu::PowerPreference::HighPerformance,
  };
  wgpu::Adapter adapter{};
  instance.RequestAdapter(&adapter_opts, wgpu::CallbackMode::AllowSpontaneous,
                          dusk::cb::adapter_request, &adapter);

  dusk::valid_or_exit(dusk::log::emit(adapter));

  bool adapter_has_f16 = false;
  {
    wgpu::SupportedFeatures features;
    adapter.GetFeatures(&features);
    for (size_t i = 0; i < features.featureCount; i++) {
      adapter_has_f16 |= wgpu::FeatureName::ShaderF16 == features.features[i];
    }
  }
  std::cout << "Adapter has shader-f16? " << (adapter_has_f16 ? "yes" : "no") << std::endl;

  // Get device
  std::vector<wgpu::FeatureName> required_features;
  wgpu::DeviceDescriptor device_desc{};
  device_desc.label = "default device";
  if (adapter_has_f16  && try_f16) {
    required_features.push_back(wgpu::FeatureName::ShaderF16);
    device_desc.requiredFeatureCount = required_features.size();
    device_desc.requiredFeatures = required_features.data();
  }
  device_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowProcessEvents,
                                    dusk::cb::device_lost);
  device_desc.SetUncapturedErrorCallback(dusk::cb::uncaptured_error);

  wgpu::Device device = adapter.CreateDevice(&device_desc);
  dusk::valid_or_exit(dusk::log::emit(device));

  return 0;
}
