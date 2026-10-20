# FFTW planner adaptation for CONVOLUTION HOST

CONVOLUTION HOST uses the FFTW 3.3.10 release source archive, SHA-256
`56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467`,
mirrored at https://deb.debian.org/debian/pool/main/f/fftw3/fftw3_3.3.10.orig.tar.gz.
The archive includes generated codelets absent from the corresponding Git tag.

Modification by Lapowire/CONVOLUTION-HOST, 20 October 2026: the CMake module
`cmake/FftBackend.cmake` builds a generated copy of `api/mapflags.c` with
`flags |= FFTW_NO_BUFFERING;` at the start of `X(mapflags)`. The verified upstream
source files remain unchanged. The adapted file retains its original copyright
and license header; the FFTW license is included alongside this notice.

Reason: JUCE's FFTW adapter requests ESTIMATE plans without NO_BUFFERING.
FFTW's `rdft2-buffered` solver allocates and frees scratch during execution,
including small transforms, violating this project's audio callback constraint.
The system-library build initially hid these calls from linker allocation
interposition. The static build exposed them in existing regression tests.

The adaptation excludes buffered solver choices before any plan is constructed.
Plan creation and destruction remain outside the audio callback. No JUCE source
patch is applied. FFTW symbols in plugin modules have hidden visibility to avoid
interposition between plugins. macOS uses vDSP and does not build FFTW.

Verification includes direct-convolution reference comparisons and audio-thread
new/delete plus Linux malloc/calloc/realloc/free interception, including 16
channels and large host blocks. These are not a certification for every host or
platform. Re-run these tests after any backend or dependency change.

Source and complete build instructions:
https://github.com/Lapowire/CONVOLUTION-HOST
