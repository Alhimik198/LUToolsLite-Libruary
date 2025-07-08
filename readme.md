LUToolsLite – Image Processing Library for 3D LUT Application and Generation
LUToolsLite is a lightweight, high-performance C++ image processing library designed to apply and generate 3D LUTs (Look-Up Tables) for color grading tasks in photography and video editing workflows.

It is built for integration into Python, C++, or other native applications via DLL/shared library usage and exposes a clean C-style API for ease of cross-language binding.

 Key Features
 1. LUT Application
Apply one or more 3D LUTs to images from disk or memory.

Supports blending LUTs (0.0–1.0 blend strength).

Chain multiple LUTs sequentially on a single image.

Adjust additional parameters:

White balance

Tint

Brightness

Contrast

Saturation

 2. Image I/O and Processing
Load and save JPEG images with built-in STB support.

Resize images to a specified dimension or fit within bounding box (preserving aspect ratio).

Efficient memory-aligned allocations for SIMD compatibility.

 3. SIMD Acceleration
Leverages AVX2 SIMD instructions for fast image processing on supported CPUs.

Automatically falls back to scalar processing if AVX2 is not available.

 4. LUT Generation
Generate custom 3D LUTs by comparing before/after image pairs.

Uses K-Nearest Neighbors to interpolate transformed color mappings.

Supports output in .CUBE format (LUT_3D_SIZE 16/32/64).

 5. Preview Support
Generate resized previews (fixed size or fit mode) with LUTs and adjustments applied.

Useful for UI integrations or quick validation.

 6. Batch File Processing
Process entire batches of image files using one or more LUTs with full parameter control.

Optional callbacks for logging and real-time progress tracking.

 7. Memory and Error Management
All allocated buffers returned to the caller must be released via LUTools_FreeMemory.

Error reporting via LUTools_GetLastErrorMessage() allows easy diagnostics.

 8. Callbacks
Log messages and progress updates via user-supplied callbacks.

 Integration
LUToolsLite is designed to be embedded into other systems. It is available as a shared library (DLL on Windows) and is easily callable via Python’s ctypes, C/C++, or any language capable of calling C-style exports.



Data Types

LogCallbackType = CFUNCTYPE(None, c_char_p, c_int, c_void_p)
ProgressCallbackType = CFUNCTYPE(None, c_float, c_void_p)

 Signature Definitions
1.  Initialization and Cleanup

lutools.LUTools_Init.restype = c_int
lutools.LUTools_Cleanup.restype = None

2.  Working with LUT

lutools.LUTools_LoadLUT.argtypes = [c_char_p, c_float, POINTER(c_int)]
lutools.LUTools_LoadLUT.restype = c_int

lutools.LUTools_UnloadLUT.argtypes = [c_int]
lutools.LUTools_UnloadLUT.restype = None

lutools.LUTools_ClearLUTs.restype = None

3.  Processing a Single File

lutools.LUTools_ProcessFile.argtypes = [
    c_char_p, c_char_p,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    LogCallbackType, c_void_p
]
lutools.LUTools_ProcessFile.restype = c_int

4.  Processing an Image in Memory

lutools.LUTools_ProcessImage.argtypes = [
    POINTER(c_ubyte), c_int, c_int, c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_ProcessImage.restype = c_int

5.  Generating Preview
5.1 With Specified Size

lutools.LUTools_GeneratePreview.argtypes = [
    POINTER(c_ubyte), c_int, c_int, c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    c_int, c_int,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_GeneratePreview.restype = c_int

5.2 Fit with Proportions Preserved

lutools.LUTools_GeneratePreviewFit.argtypes = [
    POINTER(c_ubyte), c_int, c_int, c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    c_int, c_int,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_GeneratePreviewFit.restype = c_int

6.  Resizing an Image

lutools.LUTools_ResizeImage.argtypes = [
    POINTER(c_ubyte),
    c_int, c_int, c_int,
    c_int, c_int,
    POINTER(POINTER(c_ubyte)),
    POINTER(c_int), POINTER(c_int), POINTER(c_int)
]
lutools.LUTools_ResizeImage.restype = c_int

7.  Batch File Processing

lutools.LUTools_ProcessFiles.argtypes = [
    POINTER(c_char_p), POINTER(c_char_p), c_int,
    POINTER(c_int), c_int,
    c_float, c_float, c_float, c_float, c_float,
    LogCallbackType, c_void_p
]
lutools.LUTools_ProcessFiles.restype = c_int

8.  Freeing Memory

lutools.LUTools_FreeMemory.argtypes = [POINTER(c_ubyte)]
lutools.LUTools_FreeMemory.restype = None
 Always free memory allocated for outputData using this function!

9.  Callbacks

lutools.LUTools_SetLogCallback.argtypes = [LogCallbackType, c_void_p]
lutools.LUTools_SetLogCallback.restype = None

lutools.LUTools_SetProgressCallback.argtypes = [ProgressCallbackType, c_void_p]
lutools.LUTools_SetProgressCallback.restype = None

10.  Cancel Operations

lutools.LUTools_SetCancelFlag.argtypes = [POINTER(c_int)]
lutools.LUTools_SetCancelFlag.restype = None

lutools.LUTools_IsCancelled.restype = c_int

11.  Getting the Last Error Message

lutools.LUTools_GetLastErrorMessage.argtypes = [POINTER(c_char_p)]
lutools.LUTools_GetLastErrorMessage.restype = None

12.  Creating LUT from Images

lutools.LUTools_CreateLUTFromImages.argtypes = [
    c_char_p, c_char_p,
    c_char_p,
    POINTER(c_int), c_int
]
lutools.LUTools_CreateLUTFromImages.restype = c_int

Explanation:
beforePath: Path to the "before" image
afterPath: Path to the "after" image
outputPathPrefix: Prefix for .cube files
lutSizes: Array of sizes (16, 32, 64)
numSizes: Length of the lutSizes array

 Example Usage

lut_id = c_int()
res = lutools.LUTools_LoadLUT(b"D:/LT/mylut.cube", c_float(1.0), ctypes.byref(lut_id))
if res != 0:
    err = c_char_p()
    lutools.LUTools_GetLastErrorMessage(ctypes.byref(err))
    print("Error:", err.value.decode())

 Notes:
All images must be in RGB format (3 channels).
All colors work in the range 0.0 - 1.0.
If a non-zero code is returned, always call LUTools_GetLastErrorMessage.
Use LUTools_FreeMemory to avoid memory leaks when obtaining outputData.
