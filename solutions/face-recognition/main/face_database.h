#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct FaceEntry {
    std::string name;
    std::vector<float> embedding;  // 128D L2-normalized
};

class FaceDatabase {
public:
    FaceDatabase() = default;

    // Load/save face database from JSON file
    bool load(const std::string& path);
    bool save(const std::string& path);

    // Register a face
    void addFace(const std::string& name, std::vector<float> embedding);

    // Match embedding against database, return name and score
    // Returns empty string if no match above threshold
    std::pair<std::string, float> match(const std::vector<float>& embedding, float threshold = 0.4f) const;

    // List all registered faces
    void list() const;

    int size() const { return entries_.size(); }

private:
    std::vector<FaceEntry> entries_;

    static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
    static void l2Normalize(std::vector<float>& v);
};
