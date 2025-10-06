#ifndef DTIFIBERLIB_H
#define DTIFIBERLIB_H

/**
 * DTI Fiber Visualization Library - Unified Header
 *
 * This header provides a single entry point for all DTI fiber bundle
 * visualization functionality. Simply include this file to access:
 * - TRK file reading and parsing (TrkFileReader)
 * - OpenGL fiber bundle rendering (GLFiberRenderer)
 * - OpenGL shader management (GLShaderProgram)
 *
 * Version: 2.0.0 - OpenGL Implementation
 * Author: DTI Visualization Project
 */

// Include all library modules
#include "TrkFileReader.h"
#include "GLFiberRenderer.h"
#include "GLShaderProgram.h"

// Library version information
#define DTIFIBERLIB_VERSION_MAJOR 2
#define DTIFIBERLIB_VERSION_MINOR 0
#define DTIFIBERLIB_VERSION_PATCH 0
#define DTIFIBERLIB_VERSION "2.0.0"

// Convenience namespace alias for shorter usage
namespace DTI = DTIFiberLib;

#endif // DTIFIBERLIB_H