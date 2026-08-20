#!/bin/sh
# Produce the two elementary streams the media samples feed into the TV decoder.
#
#   ./make-sample.sh <input-video> [output-dir]
#
# The samples deliberately do not demux anything: they read a raw Annex-B H.264 stream and
# a raw ADTS AAC stream, one access unit / frame at a time. Splitting the container is
# ffmpeg's job, done once, here.
#
# Nothing produced by this script is committed - see .gitignore.
set -eu

if [ $# -lt 1 ]; then
    echo "usage: $0 <input-video> [output-dir]" >&2
    exit 2
fi

input="$1"
outdir="${2:-$(dirname "$0")}"
duration="${SAMPLE_DURATION:-10}"

mkdir -p "$outdir"

# -bsf:v h264_mp4toannexb is a no-op when the source is already Annex-B, and essential when
# it came out of an MP4 (length-prefixed NALUs, SPS/PPS in the container instead of in-band).
# The samples' parser only understands start codes, and needs in-band SPS/PPS to decode.
#
# bframes=0 matters more than it looks. A raw elementary stream carries no timing, so the
# samples synthesise a timestamp from a frame counter - and that is only a presentation
# timestamp if decode order and presentation order are the same. B-frames break that: the
# counter becomes a decode timestamp, and any decoder that trusts the order it is fed will
# show frames jumping backwards. LGNC has no timestamp parameter at all, so it is the one
# where this is immediately visible.
ffmpeg -y -i "$input" -t "$duration" \
    -an -c:v libx264 -profile:v main -pix_fmt yuv420p \
    -x264-params keyint=60:min-keyint=60:scenecut=0:bframes=0 \
    -bsf:v h264_mp4toannexb -f h264 "$outdir/sample.h264"

# ADTS, because every frame then carries its own header - the sample can walk the file with
# no side-channel configuration, and the pipeline is told `"format": "adts"` at Load time.
ffmpeg -y -i "$input" -t "$duration" \
    -vn -c:a aac -ar 48000 -ac 2 \
    -f adts "$outdir/sample.aac"

echo
echo "Wrote:"
ls -la "$outdir/sample.h264" "$outdir/sample.aac"

# Raw PCM as well, for NDL DirectMedia v2 (webOS 5+): its audio types are PCM, MP3 and
# Opus - there is no AAC at all - and PCM is the one that needs neither a container nor a
# decoder to feed.
ffmpeg -y -i "$input" -t "$duration" \
    -vn -c:a pcm_s16le -ar 48000 -ac 2 \
    -f s16le "$outdir/sample.pcm"

ls -la "$outdir/sample.pcm"

# AC-3 as well. Every one of these media APIs takes it except NDL DirectMedia v2, and it is
# self-framing like ADTS, so the samples can walk it without a container.
ffmpeg -y -i "$input" -t "$duration" \
    -vn -c:a ac3 -ar 48000 -ac 2 -b:a 192k \
    -f ac3 "$outdir/sample.ac3"

ls -la "$outdir/sample.ac3"
