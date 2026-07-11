#pragma once

#include <windows.h>

// Load an embedded RT_RCDATA PNG resource and return it as a GDI+ Image*.
// Caller is responsible for deleting the returned Image*.
// Returns NULL on failure.
void* LoadEmbeddedPng(HINSTANCE hInstance, int resourceId);

// Load the embedded application icon and return HICON.
// Caller is responsible for DestroyIcon().
// size parameter: 32 for large, 16 for small.
HICON LoadEmbeddedIcon(HINSTANCE hInstance, int size);
