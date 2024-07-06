# xrlib
[![All platform CI builds [ Linux|Windows, Debug|Release, g++|clang++|cl ] [Linux|Windows, Debug|Release, cl|g++|clang++]](https://github.com/1runeberg/xrlib/actions/workflows/all_platforms.yml/badge.svg)](https://github.com/1runeberg/xrlib/actions/workflows/all_platforms.yml)

 C++20 OpenXR wrapper library designed to abstract the complexities of the OpenXR API while retaining its flexibility, allowing developers to focus on creating immersive experiences without getting bogged down by low-level details. The library is Vulkan-native and includes an optional PBR renderer, designed from the ground up with mixed reality in mind.

 **Supports: Android (aarch64), Linux (x86_64, aarch64), Windows (x86_64)**

 ## Note
 This repository currently hosts a simplified version of an internal, comprehensive rewrite of [OpenXRProvider_v2](https://github.com/1runeberg/OpenXRProvider_v2).

The internal library is significantly more advanced but is still in active development as the rewrite is in conjunction with development of a full-featured mixed reality application, scheduled for release by [Beyond Reality Labs](https://beyondreality.io). To avoid backward-incompatible changes, only the most stable components of the rewritten library are made available here.
