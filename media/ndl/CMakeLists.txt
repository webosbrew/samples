# NDL media samples.
#
# NDL is the other route to the TV decoder, and it is really two unrelated APIs that
# happened to share a name:
#
#   webOS 2.x - 3.4   libndl-directmedia2   NDL_Esplayer*  - handle-based, struct metadata
#   webOS 3.5+        libNDL_directmedia    NDL_Direct*    - global state, Open/Play/Close
#
# They share no types and no calls, so they get a directory each rather than build variants
# of one source - unlike media/smp, where the generations really are the same code.

add_subdirectory(esplayer)
