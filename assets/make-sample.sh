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
ffmpeg -y -i "$input" -t "$duration" \
    -an -c:v libx264 -profile:v main -pix_fmt yuv420p \
    -x264-params keyint=60:min-keyint=60:scenecut=0 \
    -bsf:v h264_mp4toannexb -f h264 "$outdir/sample.h264"

# ADTS, because every frame then carries its own header - the sample can walk the file with
# no side-channel configuration, and the pipeline is told `"format": "adts"` at Load time.
ffmpeg -y -i "$input" -t "$duration" \
    -vn -c:a aac -ar 48000 -ac 2 \
    -f adts "$outdir/sample.aac"

echo
echo "Wrote:"
ls -la "$outdir/sample.h264" "$outdir/sample.aac"
