#include "es_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum es_kind {
    ES_H264,
    ES_ADTS,
    ES_PCM,
} es_kind;

struct es_file {
    es_kind kind;
    const uint8_t *base;
    size_t size;
    size_t pos;
    int64_t index;

    /* H.264 */
    int fps_num;
    int fps_den;

    /* ADTS and PCM */
    int sample_rate;
    int channels;
    int object_type;

    /* PCM */
    size_t chunk_bytes;
};

/* ------------------------------------------------------------------ mapping */

static es_file *es_file_map(const char *path, es_kind kind) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "es_file: cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        fprintf(stderr, "es_file: cannot stat %s\n", path);
        close(fd);
        return NULL;
    }
    void *map = mmap(NULL, (size_t) st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
        fprintf(stderr, "es_file: cannot mmap %s\n", path);
        return NULL;
    }
    es_file *file = calloc(1, sizeof(es_file));
    if (file == NULL) {
        munmap(map, (size_t) st.st_size);
        return NULL;
    }
    file->kind = kind;
    file->base = map;
    file->size = (size_t) st.st_size;
    return file;
}

void es_file_close(es_file *file) {
    if (file == NULL) {
        return;
    }
    munmap((void *) file->base, file->size);
    free(file);
}

void es_file_rewind(es_file *file) {
    file->pos = 0;
    file->index = 0;
}

int es_file_sample_rate(const es_file *file) { return file->sample_rate; }

int es_file_channels(const es_file *file) { return file->channels; }

int es_file_aac_object_type(const es_file *file) { return file->object_type; }

/* -------------------------------------------------------------------- H.264 */

/* Returns the offset of the next Annex-B start code at or after `from`, or `size` if there
 * is none. `*code_len` receives 3 or 4. */
static size_t h264_find_start_code(const uint8_t *buf, size_t size, size_t from, int *code_len) {
    for (size_t i = from; i + 3 <= size; i++) {
        if (buf[i] != 0 || buf[i + 1] != 0) {
            continue;
        }
        if (buf[i + 2] == 1) {
            *code_len = 3;
            return i;
        }
        if (i + 4 <= size && buf[i + 2] == 0 && buf[i + 3] == 1) {
            *code_len = 4;
            return i;
        }
    }
    *code_len = 0;
    return size;
}

/*
 * Groups NAL units into access units.
 *
 * The full rule in the spec (7.4.1.2.4) is long, but for streams that come out of an
 * encoder one-slice-per-picture - which is what make-sample.sh produces - three checks
 * cover it:
 *
 *   - an access unit delimiter (type 9) always starts a new access unit;
 *   - parameter sets and SEI (7, 8, 6) start a new one if we already have picture data;
 *   - a slice (types 1-5) starts a new one if we already have picture data and its
 *     first_mb_in_slice is 0.
 *
 * first_mb_in_slice is the first ue(v) field of the slice header, so the value 0 is
 * encoded as a single '1' bit: testing the top bit of the byte after the NAL header is
 * enough, and avoids dragging in an Exp-Golomb reader.
 */
static bool h264_next(es_file *file, es_sample *out) {
    int code_len = 0;
    size_t au_start = h264_find_start_code(file->base, file->size, file->pos, &code_len);
    if (au_start >= file->size) {
        return false;
    }

    size_t cursor = au_start;
    bool have_picture = false;

    while (cursor < file->size) {
        size_t nal_start = cursor + (size_t) code_len;
        if (nal_start >= file->size) {
            cursor = file->size;
            break;
        }
        uint8_t header = file->base[nal_start];
        int type = header & 0x1f;
        bool is_slice = type >= 1 && type <= 5;

        if (nal_start > au_start) {
            bool boundary = false;
            if (type == 9) {
                boundary = true;
            } else if (have_picture && (type == 6 || type == 7 || type == 8)) {
                boundary = true;
            } else if (have_picture && is_slice && nal_start + 1 < file->size &&
                       (file->base[nal_start + 1] & 0x80) != 0) {
                boundary = true;
            }
            if (boundary) {
                break;
            }
        }
        if (is_slice) {
            have_picture = true;
        }

        int next_code_len = 0;
        size_t next = h264_find_start_code(file->base, file->size, nal_start + 1, &next_code_len);
        cursor = next;
        code_len = next_code_len;
    }

    out->data = file->base + au_start;
    out->size = cursor - au_start;
    /* 1e9 * den / num, kept in 64-bit so long streams do not drift or overflow. */
    out->pts_ns = file->index * 1000000000LL * file->fps_den / file->fps_num;

    file->pos = cursor;
    file->index++;
    return out->size > 0;
}

es_file *es_file_open_h264(const char *path, int fps_num, int fps_den) {
    if (fps_num <= 0 || fps_den <= 0) {
        fprintf(stderr, "es_file: invalid frame rate %d/%d\n", fps_num, fps_den);
        return NULL;
    }
    es_file *file = es_file_map(path, ES_H264);
    if (file == NULL) {
        return NULL;
    }
    file->fps_num = fps_num;
    file->fps_den = fps_den;
    return file;
}

/* --------------------------------------------------------------------- ADTS */

static const int adts_sample_rates[16] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000, 7350, 0, 0, 0,
};

/* Parses one ADTS header at `off`. Returns the frame length including the header, or 0 if
 * this is not a valid header. */
static size_t adts_parse(const es_file *file, size_t off, int *sample_rate, int *channels,
                         int *object_type) {
    if (off + 7 > file->size) {
        return 0;
    }
    const uint8_t *h = file->base + off;
    if (h[0] != 0xff || (h[1] & 0xf0) != 0xf0) {
        return 0;
    }
    int freq_index = (h[2] >> 2) & 0x0f;
    int rate = adts_sample_rates[freq_index];
    if (rate == 0) {
        return 0;
    }
    size_t length = ((size_t) (h[3] & 0x03) << 11) | ((size_t) h[4] << 3) | ((size_t) h[5] >> 5);
    if (length < 7 || off + length > file->size) {
        return 0;
    }
    if (sample_rate != NULL) {
        *sample_rate = rate;
    }
    if (channels != NULL) {
        *channels = ((h[2] & 0x01) << 2) | ((h[3] >> 6) & 0x03);
    }
    if (object_type != NULL) {
        /* ADTS stores audioObjectType - 1, so AAC-LC (object type 2) is written as 1. The
         * pipeline's aacInfo.profile wants the object type, hence the +1. */
        *object_type = ((h[2] >> 6) & 0x03) + 1;
    }
    return length;
}

static bool adts_next(es_file *file, es_sample *out) {
    size_t length = adts_parse(file, file->pos, NULL, NULL, NULL);
    if (length == 0) {
        return false;
    }
    out->data = file->base + file->pos;
    out->size = length;
    /* Every AAC frame carries exactly 1024 samples. */
    out->pts_ns = file->index * 1024 * 1000000000LL / file->sample_rate;

    file->pos += length;
    file->index++;
    return true;
}

es_file *es_file_open_adts(const char *path) {
    es_file *file = es_file_map(path, ES_ADTS);
    if (file == NULL) {
        return NULL;
    }
    if (adts_parse(file, 0, &file->sample_rate, &file->channels, &file->object_type) == 0) {
        fprintf(stderr, "es_file: %s does not start with an ADTS header\n", path);
        es_file_close(file);
        return NULL;
    }
    return file;
}

/* ---------------------------------------------------------------------- PCM */

static bool pcm_next(es_file *file, es_sample *out) {
    if (file->pos >= file->size) {
        return false;
    }
    size_t remaining = file->size - file->pos;
    size_t take = remaining < file->chunk_bytes ? remaining : file->chunk_bytes;
    /* Never split a sample frame across two feeds. */
    size_t frame_bytes = (size_t) file->channels * 2;
    take -= take % frame_bytes;
    if (take == 0) {
        return false;
    }

    out->data = file->base + file->pos;
    out->size = take;
    /* Timing is implicit in the byte offset - PCM carries no framing of its own. */
    out->pts_ns = (int64_t) (file->pos / frame_bytes) * 1000000000LL / file->sample_rate;

    file->pos += take;
    file->index++;
    return true;
}

es_file *es_file_open_pcm_s16le(const char *path, int sample_rate, int channels,
                                int frames_per_chunk) {
    if (sample_rate <= 0 || channels <= 0 || frames_per_chunk <= 0) {
        fprintf(stderr, "es_file: invalid PCM parameters\n");
        return NULL;
    }
    es_file *file = es_file_map(path, ES_PCM);
    if (file == NULL) {
        return NULL;
    }
    file->sample_rate = sample_rate;
    file->channels = channels;
    file->chunk_bytes = (size_t) frames_per_chunk * (size_t) channels * 2;
    return file;
}

/* ---------------------------------------------------------------- dispatch */

bool es_file_next(es_file *file, es_sample *out) {
    switch (file->kind) {
        case ES_H264:
            return h264_next(file, out);
        case ES_ADTS:
            return adts_next(file, out);
        case ES_PCM:
            return pcm_next(file, out);
        default:
            return false;
    }
}
