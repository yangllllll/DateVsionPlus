# WebSocket regression tests

These tests use local TCP connections and do not need the host application to run.
They compile the host's `core/pluginbase.cpp` and `core/plugintypes.cpp` directly,
so the sibling `DateVisionPlus` source checkout is required.

Run from an MSVC developer command prompt with the matching Qt `bin` and OpenCV
`bin` directories on `PATH`. From the SDK root:

```bat
mkdir build\websocket-tests
cd build\websocket-tests
qmake ..\..\examples\websocketserver\tests\websocketserver_tests.pro "CONFIG+=release" "CONFIG-=debug"
nmake /nologo
set QT_FATAL_WARNINGS=1
release\websocketserver_tests.exe -o test-results.txt,txt
```

Coverage includes owner-thread networking with concurrent worker execution,
coalesced automatic startup, manual start/stop overriding stale queued requests, frame-rate updates
without disconnecting clients, snapshot ownership, cached final-frame repeats,
JPEG quality invalidation, grayscale/BGRA/empty/unsupported images, per-client
backpressure, idle/partial/oversized handshakes, fragmented handshakes, and
WebSocket payload-length boundaries.

The timing diagnostic measures the first JPEG/base64/frame encoding and cached
lookup calls for a random 1920 x 1080 image. It excludes socket transfer and is
not an end-to-end frame-rate benchmark; its values vary by machine and codec
warmup. The regression assertions check bounded queues and cache reuse directly.
