#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <math.h>

#include <vector>
#include <unordered_map>
#include <algorithm>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <dxgi1_4.h>
#include <d3d12.h>
#include <windows.h>
#include <DirectXMath.h>
#include <ppl.h>

#include "external/d3dx12.h"
#include "external/stb_image.h"
#include "external/stb_perlin.h"
#include "external/dr_mp3.h"
#include "external/imgui.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define VHR(hr) if (FAILED(hr)) { assert(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }

using namespace DirectX;
// vim: set ts=4 sw=4 expandtab:
