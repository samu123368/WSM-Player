#ifndef WSM_UPDATE_FORMAT_H
#define WSM_UPDATE_FORMAT_H
#include <stdint.h>
#include <stddef.h>
#include <string>
namespace UpdateFormat {
const uint32_t MaxDolBytes = 16 * 1024 * 1024;
const size_t MaxManifestBytes = 2048;
struct Manifest {
    std::string version, url;
    uint32_t build, size;
    uint8_t hash[64];
    Manifest() : build(0), size(0) {}
};
struct Source {
    bool local;
    std::string path, host;
    uint16_t port;
    Source() : local(false), port(80) {}
};
enum Result { Valid, BadManifest, BadSignature };
bool ParseSource(const std::string &url, Source &source);
Result ParseManifest(const std::string &text, const uint8_t key[32], Manifest &out);
bool ValidDol(const uint8_t header[256], uint32_t size);
bool HttpLength(const std::string &header, uint32_t limit, uint32_t &length);
}
#endif
