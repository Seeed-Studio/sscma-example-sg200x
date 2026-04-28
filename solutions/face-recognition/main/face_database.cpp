#include "face_database.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <cstdio>

float FaceDatabase::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        dot += a[i] * b[i];
    }
    return dot;  // Already L2-normalized, so dot = cosine
}

void FaceDatabase::l2Normalize(std::vector<float>& v) {
    float norm = 0;
    for (float val : v) norm += val * val;
    norm = sqrtf(norm + 1e-10f);
    for (float& val : v) val /= norm;
}

bool FaceDatabase::load(const std::string& path) {
    // Simple text-based format: one line per face
    // name emb[0] emb[1] ... emb[127]
    std::ifstream fin(path);
    if (!fin.is_open()) return false;

    entries_.clear();
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        // Find first space after name
        size_t pos = line.find(' ');
        if (pos == std::string::npos) continue;

        FaceEntry entry;
        entry.name = line.substr(0, pos);

        // Parse 128 embedding values
        const char* p = line.c_str() + pos;
        for (int i = 0; i < 128; i++) {
            while (*p == ' ') p++;
            if (*p == '\0') break;
            char* end;
            entry.embedding.push_back(strtof(p, &end));
            p = end;
        }

        if (entry.embedding.size() == 128) {
            entries_.push_back(std::move(entry));
        }
    }
    printf("Loaded %zu faces from %s\n", entries_.size(), path.c_str());
    return true;
}

bool FaceDatabase::save(const std::string& path) {
    std::ofstream fout(path);
    if (!fout.is_open()) return false;

    for (const auto& e : entries_) {
        fout << e.name;
        for (float v : e.embedding) {
            fout << ' ' << v;
        }
        fout << '\n';
    }
    printf("Saved %zu faces to %s\n", entries_.size(), path.c_str());
    return true;
}

void FaceDatabase::addFace(const std::string& name, std::vector<float> embedding) {
    l2Normalize(embedding);
    entries_.push_back({name, std::move(embedding)});
}

std::pair<std::string, float> FaceDatabase::match(const std::vector<float>& embedding, float threshold) const {
    if (entries_.empty()) return {"", 0.0f};

    float best_score = -1.0f;
    int best_idx = -1;

    for (size_t i = 0; i < entries_.size(); i++) {
        float score = cosineSimilarity(embedding, entries_[i].embedding);
        if (score > best_score) {
            best_score = score;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0 && best_score >= threshold) {
        return {entries_[best_idx].name, best_score};
    }
    return {"", best_score};
}

void FaceDatabase::list() const {
    printf("Face database (%zu entries):\n", entries_.size());
    for (const auto& e : entries_) {
        printf("  %s\n", e.name.c_str());
    }
}
