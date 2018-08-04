#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <dxgi1_4.h>
#include <d3d12.h>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DirectXMath.h>
#include "External/d3dx12.h"
#include "External/stb_image.h"
#include "External/imgui.h"
#include "External/tiny_obj_loader.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define VHR(hr) if (FAILED(hr)) { assert(0); }
#define SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }
