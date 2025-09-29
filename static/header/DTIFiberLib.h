#ifndef DTIFIBERLIB_H
#define DTIFIBERLIB_H

/**
 * DTI Fiber Visualization Library - Unified Header
 * 
 * This header provides a single entry point for all DTI fiber bundle
 * visualization functionality. Simply include this file to access:
 * - TRK file reading and parsing (TrkFileReader)
 * - Fiber bundle rendering (FiberBundleRenderer) 
 * - VTK rendering management (DTIFiberRenderer)
 * 
 * Version: 1.0.0
 * Author: DTI Visualization Project
 */

// Include all library modules
#include "TrkFileReader.h"
#include "FiberBundleRenderer.h"
#include "DTIRenderer.h"

// Library version information
#define DTIFIBERLIB_VERSION_MAJOR 1
#define DTIFIBERLIB_VERSION_MINOR 0
#define DTIFIBERLIB_VERSION_PATCH 0
#define DTIFIBERLIB_VERSION "1.0.0"

// Convenience namespace alias for shorter usage
namespace DTI = DTIFiberLib;

#endif // DTIFIBERLIB_H